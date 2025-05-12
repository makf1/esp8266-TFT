#include <Wire.h>
#include <WiFi.h>
#include <GyverBME280.h>
#include <SparkFunCCS811.h>
#include <TFT_eSPI.h>
#include <SPI.h>
// #include <FastBot2.h>

//============================== НАСТРОЙКИ ==================================//
// Wi-Fi
#define WIFI_SSID "x"
#define WIFI_PASS "x"
#define HOSTNAME "esp-meteo"

// Настройка статического IP
IPAddress staticIP(192, 168, 1, 144);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Telegram
// #define BOT_TOKEN "x"
// #define CHAT_ID "x"

// Датчики
#define I2C_SDA 21
#define I2C_SCL 22
#define CCS811_ADDR 0x5A

// Пороги оповещений
// #define CO2_ALERT_LEVEL 1400
// #define CO2_OK_LEVEL 1200

//========================= ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ==========================//
GyverBME280 bme;
CCS811 ccs(CCS811_ADDR);
// FastBot2 bot;
TFT_eSPI tft = TFT_eSPI();

struct SensorData {
  float temp;
  float humi;
  float pres;
  uint16_t eco2;
  uint16_t tvoc;
} sensorData;

#define HIST_SIZE 24
struct {
  uint16_t co2[HIST_SIZE];
  uint8_t idx = 0;
} history;

volatile bool co2Alert = false;
SemaphoreHandle_t i2cMutex;
SemaphoreHandle_t dataMutex;

// Прототипы функций
void vBMETask(void *pv);
void vCCSTask(void *pv);
void vWiFiTask(void *pv);
void vDisplayTask(void *pv);
void vBotTask(void *pv);
void i2cScan();
void checkThresholds();
String formatData();

// Обработчик сообщений Telegram
// void botHandler(FB_msg msg) {
//   if (msg.text == "/sensors") {
//     sendSensorData(msg.chatID); // Используем новую функцию
//   }
//   else if (msg.text == "/chart") {
//     String chart = "📊 История CO2:\n";
//     xSemaphoreTake(dataMutex, portMAX_DELAY); // Защита данных
//     for (int i = 0; i < HIST_SIZE; i++) {
//       chart += String(history.co2[i]) + " ";
//     }
//     xSemaphoreGive(dataMutex);
//     bot.sendMessage(chart, msg.chatID);
//   }
//   else if (msg.text == "/help") {
//     String help = "📋 Доступные команды:\n"
//                   "/sensors - текущие данные\n"
//                   "/chart - история CO2\n"
//                   "/help - эта справка";
//     bot.sendMessage(help, msg.chatID);
//   }
// }

void setup() {
  Serial.begin(115200);
  
  // Инициализация I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  // SPI.begin(TFT_SCLK, TFT_MOSI, -1, TFT_CS); // Для ESP32
  Wire.setClock(100000);
  i2cMutex = xSemaphoreCreateMutex();
  dataMutex = xSemaphoreCreateMutex();

  // Инициализация дисплея
  tft.init();
  tft.fillScreen(TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("Hello World!", 50, 100);
  delay(2000);

  // Настройка Telegram бота
  // bot.setToken(BOT_TOKEN);
  // bot.attachMsg(botHandler);
  // bot.showTyping(true); // Показывать индикатор набора

  // Создание задач
  xTaskCreatePinnedToCore(vBMETask, "BME280", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(vCCSTask, "CCS811", 4096, NULL, 2, NULL, 0);
  xTaskCreatePinnedToCore(vWiFiTask, "WiFi", 4096, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(vDisplayTask, "Display", 8192, NULL, 3, NULL, 1);
  // xTaskCreatePinnedToCore(vBotTask, "Bot", 8192, NULL, 1, NULL, 1);

  i2cScan();
}

void loop() { vTaskDelete(NULL); }

// Реализация задач
void vBMETask(void *pv) {
  if(xSemaphoreTake(i2cMutex, pdMS_TO_TICKS(1000))) {
    if(!bme.begin()) {
      Serial.println("[BME] Error!");
      vTaskDelete(NULL);
    }
    xSemaphoreGive(i2cMutex);
  }
  
  for(;;) {
    if(xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
      float t = bme.readTemperature();
      float h = bme.readHumidity();
      float p = bme.readPressure() / 133;

      Serial.print("\nBME-> Temp: "); Serial.print(String(t)); Serial.print("[C] | Hum: "); Serial.print(String(h)); Serial.print("[%] | Pres: "); Serial.print(String(p)); Serial.println("[mmHg]");
      
      if(xSemaphoreTake(dataMutex, portMAX_DELAY)) {
        sensorData.temp = t;
        sensorData.humi = h;
        sensorData.pres = p;
        xSemaphoreGive(dataMutex);
      }
      xSemaphoreGive(i2cMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void vCCSTask(void *pv) {
  vTaskDelay(pdMS_TO_TICKS(2000));
  
  if(xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
    if(!ccs.begin()) {
      Serial.print("[CCS] Error: 0x");
      Serial.println(ccs.getErrorRegister(), HEX);
      vTaskDelete(NULL);
    }
    ccs.setDriveMode(1);
    xSemaphoreGive(i2cMutex);
  }

  for(;;) {
    if(xSemaphoreTake(i2cMutex, portMAX_DELAY)) {
      if(ccs.dataAvailable()) {
        ccs.setEnvironmentalData(sensorData.humi, sensorData.temp);
        ccs.readAlgorithmResults();
        
        if(xSemaphoreTake(dataMutex, portMAX_DELAY)) {
          sensorData.eco2 = ccs.getCO2();
          sensorData.tvoc = ccs.getTVOC();

          Serial.print("CCS-> CO2: "); Serial.print(String(ccs.getCO2())); Serial.print("[ppm] | tVOC: "); Serial.print(String(ccs.getTVOC())); Serial.println("[ppb]");
          
          history.co2[history.idx] = sensorData.eco2;
          history.idx = (history.idx + 1) % HIST_SIZE;
          xSemaphoreGive(dataMutex);
          
          // checkThresholds();
        }
      }
      xSemaphoreGive(i2cMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(2000));
  }
}

void vWiFiTask(void *pv) {
  WiFi.config(staticIP, gateway, subnet);
  WiFi.setHostname(HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  for(uint8_t i=0; i<15; i++) {
    if(WiFi.status() == WL_CONNECTED) break;
    Serial.print(".");
    vTaskDelay(pdMS_TO_TICKS(500));
  }

  if(WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi OK! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi FAIL!");
  }

  for(;;) {
    if(WiFi.status() != WL_CONNECTED) WiFi.reconnect();
    vTaskDelay(pdMS_TO_TICKS(30000));
  }
}

void vDisplayTask(void *pv) {
  // Инициализация дисплея без использования RESET
  tft.init();
  tft.setRotation(0); // Попробуйте разные значения (0-3)
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM); // Выравнивание по верхнему левому краю

  for(;;) {
    if(xSemaphoreTake(dataMutex, portMAX_DELAY)) {
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0, 0);
      tft.printf("Temp:  %.1f C\n", sensorData.temp);
      tft.printf("Hum:   %.1f %%\n", sensorData.humi);
      tft.printf("Pres:  %.1f mmHg\n", sensorData.pres);
      tft.printf("CO2:   %d ppm\n", sensorData.eco2);
      tft.printf("TVOC:  %d ppb", sensorData.tvoc);
      xSemaphoreGive(dataMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// // Новая функция для отправки данных
// void sendSensorData(const String& chatID) {
//   xSemaphoreTake(dataMutex, portMAX_DELAY);
//   String msg = "🌡️ Температура: " + String(sensorData.temp,1) + "°C\n";
//   msg += "💧 Вологість: " + String(sensorData.humi,1) + "%\n";
//   msg += "🔄 Тиск: " + String(sensorData.pres,1) + " мм.рт.ст.\n";
//   msg += "🌫️ CO₂: " + String(sensorData.eco2) + " ppm\n";
//   msg += "🧪 ЛОС: " + String(sensorData.tvoc) + " ppb";
//   xSemaphoreGive(dataMutex);
  
//   bot.sendMessage(msg, chatID);
// }

// void vBotTask(void *pv) {
//   for(;;) {
//     bot.tick();
//     vTaskDelay(100 / portTICK_PERIOD_MS);
//   }
// }

// // Вспомогательные функции
// void checkThresholds() {
//   if(sensorData.eco2 > CO2_ALERT_LEVEL && !co2Alert) {
//     bot.sendMessage("⚠️ CO2 Alert: " + String(sensorData.eco2) + "ppm", CHAT_ID);
//     co2Alert = true;
//   }
//   else if(sensorData.eco2 < CO2_OK_LEVEL && co2Alert) {
//     co2Alert = false;
//   }
// }

// String formatData() {
//   String msg = "🌡️ Temp: " + String(sensorData.temp,1) + "C\n";
//   msg += "💧 Hum: " + String(sensorData.humi,1) + "%\n";
//   msg += "🔄 Pres: " + String(sensorData.pres,1) + "mmHg\n";
//   msg += "🌫️ CO2: " + String(sensorData.eco2) + "ppm\n";
//   msg += "🧪 TVOC: " + String(sensorData.tvoc) + "ppb";
//   return msg;
// }

void i2cScan() {
  Serial.println("\nI2C Scanner:");
  for(uint8_t addr=1; addr<127; addr++) {
    Wire.beginTransmission(addr);
    if(Wire.endTransmission() == 0) {
      Serial.printf("Found: 0x%02X\n", addr);
    }
  }
}
