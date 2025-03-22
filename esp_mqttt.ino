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
const char* mqtt_server    = "a51ad753198b41b3a4c2f4488d3e409d.s1.eu.hivemq.cloud";
const int   mqtt_port      = 8883;
const char* mqtt_username  = "haidaramrurusdan";
const char* mqtt_password  = "h18082746R";

WiFiClientSecure mqttWiFiClient;
PubSubClient mqttClient(mqttWiFiClient);

// ===================== Konfigurasi UART =====================
#define UART_PORT_NUM         UART_NUM_1
#define UART_BAUD_RATE        115200
#define UART_TX_PIN           GPIO_NUM_17
#define UART_RX_PIN           GPIO_NUM_16
#define UART_BUF_SIZE         250

// ===================== Konfigurasi Wake-Up =====================
#define WAKEUP_PIN            GPIO_NUM_4

// Timeout inaktivitas UART (dalam milidetik)
#define UART_INACTIVITY_TIMEOUT_MS   20000

// Handle timer inaktivitas
static TimerHandle_t inactivity_timer = NULL;

// Flag untuk deep sleep dan update waktu
volatile bool shouldDeepSleep = false;
volatile bool updateTimeRequested = false;

// Variabel kredensial WiFi
String WIFI_SSID = "";
String WIFI_PASSWORD = "";

// Queue UART
QueueHandle_t uart_queue = NULL;

// Variabel untuk data sensor yang akan dikirim ke MQTT
String sensorDataBuffer = "";
unsigned long lastSensorDataMillis = 0;

// ===================== Fungsi MQTT =====================
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

void publishSensorData(String data) {
  // Pastikan WiFi terkoneksi terlebih dahulu
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi belum terkoneksi. Menghubungkan...");
    WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
    int retry = 0;
    while (WiFi.status() != WL_CONNECTED && retry < 20) {
      delay(500);
      Serial.print(".");
      retry++;
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi terkoneksi.");
    } else {
      Serial.println("\nGagal menghubungkan WiFi. Batal publish data.");
      return;
    }
  }
  // Pastikan koneksi MQTT tersedia
  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  if (mqttClient.publish("sensor/cuaca", data.c_str())) {
    Serial.print("Dipublish ke sensor/cuaca: ");
    Serial.println(data);
  } else {
    Serial.println("Gagal publish ke sensor/cuaca");
  }
}

// ===================== Fungsi Update Waktu =====================
void updateTimeFromServer() {
  if (WiFi.status() != WL_CONNECTED) {
    if (WIFI_SSID != "") {
      Serial.println("Menghubungkan ke WiFi...");
      WiFi.begin(WIFI_SSID.c_str(), WIFI_PASSWORD.c_str());
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
    } else {
      Serial.println("Kredensial WiFi belum disetel. Tidak dapat update waktu.");
      return;
    }
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
            Serial.print("Waktu yang didapat: ");
            Serial.println(timeBuffer);

            uart_write_bytes(UART_PORT_NUM, timeBuffer, strlen(timeBuffer));
            Serial.println("Waktu dikirim ke STM32.");
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

// ===================== Fungsi Parsing Data UART =====================
// Fungsi ini memecah string yang diterima berdasarkan koma dan memproses masing-masing token.
void processReceived(String received) {
  int start = 0;
  while (true) {
    int commaIndex = received.indexOf(',', start);
    String token;
    if (commaIndex == -1) {
      token = received.substring(start);
    } else {
      token = received.substring(start, commaIndex);
    }
    token.trim();
    if (token.length() > 0) {
      // Jika token adalah perintah timeupdate
      if (token.equals("timeupdate")) {
        Serial.println("Perintah timeupdate diterima, meminta update waktu.");
        updateTimeRequested = true;
      }
      // Jika token adalah kredensial WiFi
      else if (token.startsWith("ssid:")) {
        String newSSID = token.substring(5);
        if (newSSID != WIFI_SSID) {
          WIFI_SSID = newSSID;
          Serial.print("SSID diperbarui: ");
          Serial.println(WIFI_SSID);
        }
      } 
      else if (token.startsWith("pass:")) {
        String newPass = token.substring(5);
        if (newPass != WIFI_PASSWORD) {
          WIFI_PASSWORD = newPass;
          Serial.print("Password diperbarui: ");
          Serial.println(WIFI_PASSWORD);
        }
      }
      // Token dianggap data sensor
      else {
        // Akumulasi data sensor ke buffer
        if (sensorDataBuffer.length() > 0) {
          sensorDataBuffer += ",";
        }
        sensorDataBuffer += token;
        lastSensorDataMillis = millis();
      }
    }
    if (commaIndex == -1)
      break;
    start = commaIndex + 1;
  }
}

// ===================== Task untuk Event UART =====================
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
            
            String received = String((char*)data_buffer, len);
            received.trim();
            processReceived(received);
            
            // Reset timer inaktivitas
            if (inactivity_timer != NULL) {
              if (xTimerReset(inactivity_timer, 0) != pdPASS) {
                Serial.println("Gagal mereset timer inaktivitas");
              }
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

// ===================== Deep Sleep =====================
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

// ===================== Setup dan Loop =====================
void setup() {
  Serial.begin(115200);
  while (!Serial) { }
  Serial.println("Booting...");

  pinMode(WAKEUP_PIN, INPUT_PULLUP);

  // Inisialisasi UART
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
  
  Serial.println("Menunggu kredensial WiFi dan perintah dari UART...");

  // Konfigurasi MQTT
  mqttWiFiClient.setInsecure();
  mqttClient.setServer(mqtt_server, mqtt_port);
}

void loop() {
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  if (shouldDeepSleep) {
    shouldDeepSleep = false;
    enter_deep_sleep();
  }
  
  if (updateTimeRequested) {
    updateTimeRequested = false;
    xTaskCreatePinnedToCore(updateTimeTask, "updateTimeTask", 8192, NULL, 10, NULL, 0);
  }
  
  // Jika tidak ada data sensor baru selama 1 detik, publish data sensor
  if (sensorDataBuffer.length() > 0 && (millis() - lastSensorDataMillis) > 1000) {
    publishSensorData(sensorDataBuffer);
    sensorDataBuffer = "";
  }
  
  delay(10);
}

