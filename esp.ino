#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// ===================== Konfigurasi UART =====================
#define UART_PORT_NUM         UART_NUM_1
#define UART_BAUD_RATE        115200
#define UART_TX_PIN           GPIO_NUM_17
#define UART_RX_PIN           GPIO_NUM_16
#define UART_BUF_SIZE         100

// ===================== Konfigurasi Wake-Up =====================
// Menggunakan pin IO4 sebagai sumber wake‑up (RTC-capable)
#define WAKEUP_PIN            GPIO_NUM_4

// Timeout inaktivitas UART (dalam milidetik)
#define UART_INACTIVITY_TIMEOUT_MS   10000

// Handle timer inaktivitas
static TimerHandle_t inactivity_timer = NULL;

// Flag untuk memicu deep sleep (diubah oleh timer callback)
volatile bool shouldDeepSleep = false;

// Flag untuk memicu update waktu
volatile bool updateTimeRequested = false;

// ===================== WiFi Credentials =====================
const char* WIFI_SSID = "SAKTI";
const char* WIFI_PASSWORD = "sakti0808";

// ===================== Variabel Global UART Queue =====================
QueueHandle_t uart_queue = NULL;

// ===================== Fungsi Deep Sleep =====================
void enter_deep_sleep() {
  Serial.println("Tidak ada aktivitas UART. Memasuki mode deep sleep...");

  // Matikan koneksi WiFi sebelum deep sleep
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.flush();

  // Konfigurasi sumber wake‑up eksternal
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);
  // Delay singkat agar pesan tercetak
  vTaskDelay(pdMS_TO_TICKS(100));
  Serial.println("ESP32 masuk deep sleep sekarang.");
  esp_deep_sleep_start();
}

// ===================== Callback Timer Inaktivitas =====================
// Jangan lakukan operasi I/O di sini; cukup set flag untuk diproses di loop()
void inactivity_timer_callback(TimerHandle_t xTimer) {
  shouldDeepSleep = true;
}

// Fungsi untuk mereset timer inaktivitas setiap kali ada data masuk
void reset_inactivity_timer() {
  if (inactivity_timer != NULL) {
    if (xTimerReset(inactivity_timer, 0) != pdPASS) {
      Serial.println("Gagal mereset timer inaktivitas");
    }
  }
}

// ===================== Fungsi Update Waktu dari Server BMKG =====================
void updateTimeFromServer() {
  // Pastikan koneksi WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Menghubungkan ke WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
      delay(500);
      Serial.print(".");
      retry++;
    }
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\nGagal menghubungkan ke WiFi");
      return;
    }
    Serial.println("\nWiFi terkoneksi.");
  }

  // Membuat klien HTTPS (tidak melakukan verifikasi sertifikat)
  WiFiClientSecure client;
  client.setInsecure();  // Hanya untuk keperluan testing
  HTTPClient https;
  const char* serverURL = "https://jam.bmkg.go.id/JamServer.php";

  Serial.println("Mengambil data waktu dari server BMKG...");
  if (https.begin(client, serverURL)) {
    int httpCode = https.GET();
    if (httpCode > 0) {
      if (httpCode == HTTP_CODE_OK) {
        String payload = https.getString();
        Serial.println("Response dari server:");
        Serial.println(payload);

        // Cari bagian payload yang berisi "new Date("
        String searchStr = "new Date(";
        int idx = payload.indexOf(searchStr);
        if (idx != -1) {
          int startIdx = idx + searchStr.length();
          int endIdx = payload.indexOf(")", startIdx);
          if (endIdx != -1) {
            String dateStr = payload.substring(startIdx, endIdx);
            Serial.print("Date string: ");
            Serial.println(dateStr);
            // Contoh dateStr: "2025,02-1,09,17,04,53"

            // Konversi String ke array char untuk proses tokenisasi
            char dateChar[50];
            dateStr.toCharArray(dateChar, sizeof(dateChar));
            int values[6] = {0};
            int i = 0;
            char *token = strtok(dateChar, ",");
            while (token != NULL && i < 6) {
              if (i == 1) { // Token untuk bulan (misal "02-1")
                char *dash = strchr(token, '-');
                if (dash != NULL) {
                  char monthPart[10];
                  int len = dash - token;
                  strncpy(monthPart, token, len);
                  monthPart[len] = '\0';
                  values[i] = atoi(monthPart);
                } else {
                  values[i] = atoi(token);
                }
              } else {
                values[i] = atoi(token);
              }
              token = strtok(NULL, ",");
              i++;
            }
            // Urutan: tahun, bulan, tanggal, jam, menit, detik
            int tahun  = values[0];
            int bulan  = values[1];
            int tanggal = values[2];
            int jam    = values[3];
            int menit  = values[4];
            int detik  = values[5];

            // Format pesan untuk dikirim ke STM32:
            // Format: "tanggal bulan tahun jam menit detik"
            char timeBuffer[50];
            sprintf(timeBuffer, "x%02d %02d %04d %02d %02d %02d", tanggal, bulan, tahun, jam, menit, detik);
            Serial.print("Waktu yang dikirim: ");
            Serial.println(timeBuffer);

            // Kirim data waktu melalui UART ke STM32
            uart_write_bytes(UART_PORT_NUM, timeBuffer, strlen(timeBuffer));
            uart_write_bytes(UART_PORT_NUM, "\n", 1); // Tambahan newline
          } else {
            Serial.println("Tidak menemukan penutup ')' untuk tanggal.");
          }
        } else {
          Serial.println("Format waktu tidak ditemukan dalam payload.");
        }
      } else {
        Serial.printf("Kode HTTP: %d\n", httpCode);
      }
    } else {
      Serial.printf("Gagal mendapatkan response, error: %s\n", https.errorToString(httpCode).c_str());
    }
    https.end();
  } else {
    Serial.println("Gagal inisialisasi HTTP.");
  }
}

// Task untuk update waktu, dijalankan pada core 0
void updateTimeTask(void *pvParameters) {
  updateTimeFromServer();
  vTaskDelete(NULL);
}

// ===================== Task untuk Menangani Event UART =====================
void uart_event_task(void *pvParameters) {
  // Ambil handle antrian event UART dari variabel global
  QueueHandle_t local_uart_queue = *(QueueHandle_t *)pvParameters;
  uart_event_t event;
  uint8_t* data_buffer = (uint8_t*) malloc(UART_BUF_SIZE);
  if (data_buffer == NULL) {
    Serial.println("Gagal mengalokasikan buffer UART");
    vTaskDelete(NULL);
  }
  
  for (;;) {
    // Bersihkan buffer sebelum menerima data
    memset(data_buffer, 0, UART_BUF_SIZE);
    if (xQueueReceive(local_uart_queue, (void *)&event, portMAX_DELAY)) {
      switch (event.type) {
        case UART_DATA: {
          int len = uart_read_bytes(UART_PORT_NUM, data_buffer, event.size, pdMS_TO_TICKS(100));
          if (len > 0) {
            Serial.print("Data diterima: ");
            Serial.write(data_buffer, len);
            Serial.println();
            // Reset timer inaktivitas karena ada data yang diterima
            reset_inactivity_timer();

            // Jika data mengandung kata "timeupdate", set flag untuk update waktu
            if (strstr((char*)data_buffer, "timeupdate") != NULL) {
              Serial.println("Perintah timeupdate diterima, meminta update waktu.");
              updateTimeRequested = true;
            }
          }
          break;
        }
        case UART_FIFO_OVF:
          Serial.println("UART FIFO overflow");
          uart_flush_input(UART_PORT_NUM);
          xQueueReset(local_uart_queue);
          break;
        case UART_BUFFER_FULL:
          Serial.println("UART ring buffer penuh");
          uart_flush_input(UART_PORT_NUM);
          xQueueReset(local_uart_queue);
          break;
        case UART_PARITY_ERR:
          Serial.println("UART parity error");
          break;
        case UART_FRAME_ERR:
          Serial.println("UART frame error");
          break;
        default:
          Serial.printf("Event UART lainnya: %d\n", event.type);
          break;
      }
    }
  }
  free(data_buffer);
  vTaskDelete(NULL);
}

// ===================== Setup dan Loop =====================
void setup() {
  // Inisialisasi Serial Monitor untuk debug
  Serial.begin(115200);
  while (!Serial) { }
  Serial.println("Booting...");

  // Konfigurasi WAKEUP_PIN sebagai input dengan pull-up
  pinMode(WAKEUP_PIN, INPUT_PULLUP);

  // Buat timer inaktivitas
  inactivity_timer = xTimerCreate("inactivity_timer",
                                    pdMS_TO_TICKS(UART_INACTIVITY_TIMEOUT_MS),
                                    pdFALSE, // timer satu kali
                                    (void *)0,
                                    inactivity_timer_callback);
  if (inactivity_timer == NULL) {
    Serial.println("Gagal membuat timer inaktivitas");
  } else {
    xTimerStart(inactivity_timer, 0);
  }

  // Konfigurasi UART
  uart_config_t uart_config = {
    .baud_rate = UART_BAUD_RATE,
    .data_bits = UART_DATA_8_BITS,
    .parity    = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_APB,
  };
  uart_param_config(UART_PORT_NUM, &uart_config);
  uart_set_pin(UART_PORT_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

  // Install driver UART dengan buffer dan antrian event
  // Menggunakan variabel global uart_queue agar tetap valid selama runtime
  uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 20, &uart_queue, 0);
  if (uart_queue == NULL) {
    Serial.println("Gagal membuat UART queue");
  }

  // Buat task untuk menangani event UART
  // Karena uart_queue adalah variabel global, kita kirimkan alamatnya
  xTaskCreate(uart_event_task, "uart_event_task", 4096, (void *)&uart_queue, 12, NULL);
}

void loop() {
  // Cek flag deep sleep setiap 1 detik
  if (shouldDeepSleep) {
    shouldDeepSleep = false;
    enter_deep_sleep();
  }
  
  // Jika ada permintaan update waktu, buat task update waktu di core 0
  if (updateTimeRequested) {
    updateTimeRequested = false;
    xTaskCreatePinnedToCore(updateTimeTask, "updateTimeTask", 8192, NULL, 10, NULL, 0);
  }
  
  vTaskDelay(pdMS_TO_TICKS(1000));
}
