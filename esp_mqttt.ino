#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <HTTPClient.h>

// ===================== Konfigurasi HiveMQ Cloud =====================
const char* mqtt_server    = "a51ad753198b41b3a4c2f4488d3e409d.s1.eu.hivemq.cloud";  // Ganti dengan URL cluster Anda
const int   mqtt_port      = 8883;                             // Port TLS
const char* mqtt_username  = "haidaramrurusdan";                // Username HiveMQ Cloud
const char* mqtt_password  = "h18082746R";                      // Password HiveMQ Cloud 

WiFiClientSecure mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);

// ===================== Konfigurasi UART =====================
#define UART_PORT_NUM         UART_NUM_1
#define UART_BAUD_RATE        115200
#define UART_TX_PIN           GPIO_NUM_17
#define UART_RX_PIN           GPIO_NUM_16
#define UART_BUF_SIZE         100

// ===================== Konfigurasi Wake-Up =====================
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
const char* WIFI_SSID = "Si Fulan";
const char* WIFI_PASSWORD = "h18082746";

// ===================== Variabel Global UART Queue =====================
QueueHandle_t uart_queue = NULL;

// ===================== Variabel Global untuk Data ke MQTT =====================
volatile bool sendDataToMQTT = false;
String uartReceivedString = "";

// Fungsi untuk menghubungkan ke MQTT HiveMQ Cloud
void reconnectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Menghubungkan ke MQTT HiveMQ Cloud...");
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
      Serial.println(" terhubung.");
    } else {
      Serial.print("Gagal, rc=");
      Serial.print(mqttClient.state());
      Serial.println(". Mencoba lagi dalam 5 detik...");
      delay(5000);
    }
  }
}

// Fungsi untuk mempublikasikan data sensor ke topik-topik MQTT
void publishSensorData(String data) {
  // Topik-topik yang akan dipublish
  const char* topics[9] = {
    "sensor/waktu",
    "sensor/suhu",
    "sensor/kelembaban",
    "sensor/arah_angin",
    "sensor/kecepatan_angin",
    "sensor/tekanan_udara",
    "sensor/radiasi_matahari",
    "sensor/curah_hujan",
    "sensor/water_level"
  };

  // Pisahkan data berdasarkan koma
  int startIdx = 0;
  int commaIdx = -1;
  String token;
  for (int i = 0; i < 9; i++) {
    if (i < 8) {
      commaIdx = data.indexOf(',', startIdx);
      if (commaIdx == -1) {
        Serial.println("Format data tidak sesuai.");
        return;
      }
      token = data.substring(startIdx, commaIdx);
      startIdx = commaIdx + 1;
    } else {
      token = data.substring(startIdx);
    }
    token.trim();  // Menghapus spasi di awal dan akhir
    if(mqttClient.publish(topics[i], token.c_str())){
      Serial.print("Dipublish ke ");
      Serial.print(topics[i]);
      Serial.print(": ");
      Serial.println(token);
    } else {
      Serial.print("Gagal publish ke ");
      Serial.println(topics[i]);
    }
  }
}

// Fungsi untuk update waktu dari server BMKG dan publikasikan ke MQTT
void updateTimeFromServer() {
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

  WiFiClientSecure client;
  client.setInsecure();
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

        String searchStr = "new Date(";
        int idx = payload.indexOf(searchStr);
        if (idx != -1) {
          int startIdx = idx + searchStr.length();
          int endIdx = payload.indexOf(")", startIdx);
          if (endIdx != -1) {
            String dateStr = payload.substring(startIdx, endIdx);
            Serial.print("Date string: ");
            Serial.println(dateStr);

            char dateChar[50];
            dateStr.toCharArray(dateChar, sizeof(dateChar));
            int values[6] = {0};
            int i = 0;
            char *token = strtok(dateChar, ",");
            while (token != NULL && i < 6) {
              if (i == 1) {
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
            int tahun  = values[0];
            int bulan  = values[1];
            int tanggal = values[2];
            int jam    = values[3];
            int menit  = values[4];
            int detik  = values[5];

            char timeBuffer[50];
            sprintf(timeBuffer, "x%02d %02d %04d %02d %02d %02d", tanggal, bulan, tahun, jam, menit, detik);
            Serial.print("Waktu yang dikirim: ");
            Serial.println(timeBuffer);

            // Publikasikan waktu ke topik sensor/waktu
            if (!mqttClient.connected()) {
              reconnectMQTT();
            }
            mqttClient.publish("sensor/waktu", timeBuffer);
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

void updateTimeTask(void *pvParameters) {
  updateTimeFromServer();
  vTaskDelete(NULL);
}

void uart_event_task(void *pvParameters) {
  QueueHandle_t local_uart_queue = *(QueueHandle_t *)pvParameters;
  uart_event_t event;
  uint8_t* data_buffer = (uint8_t*) malloc(UART_BUF_SIZE);
  if (data_buffer == NULL) {
    Serial.println("Gagal mengalokasikan buffer UART");
    vTaskDelete(NULL);
  }
  
  for (;;) {
    memset(data_buffer, 0, UART_BUF_SIZE);
    if (xQueueReceive(local_uart_queue, (void *)&event, portMAX_DELAY)) {
      switch (event.type) {
        case UART_DATA: {
          int len = uart_read_bytes(UART_PORT_NUM, data_buffer, event.size, pdMS_TO_TICKS(100));
          if (len > 0) {
            Serial.print("Data diterima: ");
            Serial.write(data_buffer, len);
            Serial.println();
            // Reset timer inaktivitas
            if (inactivity_timer != NULL) {
              if (xTimerReset(inactivity_timer, 0) != pdPASS) {
                Serial.println("Gagal mereset timer inaktivitas");
              }
            }

            if (strstr((char*)data_buffer, "timeupdate") != NULL) {
              Serial.println("Perintah timeupdate diterima, meminta update waktu.");
              updateTimeRequested = true;
            }

            // Konversi buffer ke String
            uartReceivedString = String((char*)data_buffer, len);
            sendDataToMQTT = true;
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

void enter_deep_sleep() {
  Serial.println("Tidak ada aktivitas UART. Memasuki mode deep sleep...");
  WiFi.disconnect();
  WiFi.mode(WIFI_OFF);
  Serial.flush();
  esp_sleep_enable_ext0_wakeup(WAKEUP_PIN, 0);
  vTaskDelay(pdMS_TO_TICKS(100));
  Serial.println("ESP32 masuk deep sleep sekarang.");
  esp_deep_sleep_start();
}

void inactivity_timer_callback(TimerHandle_t xTimer) {
  shouldDeepSleep = true;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { }
  Serial.println("Booting...");

  // Konfigurasi WAKEUP_PIN
  pinMode(WAKEUP_PIN, INPUT_PULLUP);

  // --- PENTING: Inisialisasi UART terlebih dahulu ---
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
  uart_driver_install(UART_PORT_NUM, UART_BUF_SIZE * 2, 0, 20, &uart_queue, 0);
  if (uart_queue == NULL) {
    Serial.println("Gagal membuat UART queue");
  }
  xTaskCreate(uart_event_task, "uart_event_task", 4096, (void *)&uart_queue, 12, NULL);

  // Buat timer inaktivitas
  inactivity_timer = xTimerCreate("inactivity_timer",
                                  pdMS_TO_TICKS(UART_INACTIVITY_TIMEOUT_MS),
                                  pdFALSE,
                                  (void *)0,
                                  inactivity_timer_callback);
  if (inactivity_timer == NULL) {
    Serial.println("Gagal membuat timer inaktivitas");
  } else {
    xTimerStart(inactivity_timer, 0);
  }

  // Hubungkan ke WiFi pada setup
  Serial.println("Menghubungkan ke WiFi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int retry = 0;
  while(WiFi.status() != WL_CONNECTED && retry < 20) {
    delay(500);
    Serial.print(".");
    retry++;
  }
  if(WiFi.status() == WL_CONNECTED){
    Serial.println("\nWiFi terkoneksi!");
  } else {
    Serial.println("\nGagal menghubungkan ke WiFi.");
  }

  // Konfigurasi MQTT
  mqttWiFiClient.setInsecure(); // Non-aktifkan verifikasi sertifikat TLS
  mqttClient.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  if (shouldDeepSleep) {
    shouldDeepSleep = false;
    enter_deep_sleep();
  }
  
  if (updateTimeRequested) {
    updateTimeRequested = false;
    xTaskCreatePinnedToCore(updateTimeTask, "updateTimeTask", 8192, NULL, 10, NULL, 0);
  }
  
  if (sendDataToMQTT) {
    sendDataToMQTT = false;
    // Publikasikan data yang diterima melalui UART ke MQTT
    publishSensorData(uartReceivedString);
  }
  
  delay(10);
}
