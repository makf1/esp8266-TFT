#include <Wire.h>
#include <GyverBME280.h>
#include <TFT_eSPI.h>
#include <SparkFunCCS811.h> //Click here to get the library: http://librarymanager/All#SparkFun_CCS811

// Глобальные объекты классов
CCS811 myCCS811(0x5A);
TFT_eSPI tft = TFT_eSPI();
GyverBME280 bme;

// Настройки дуги
#define CENTER_X 120
#define CENTER_Y 120
#define ARC_RADIUS 120
#define ARC_WIDTH 113
#define ARC_COLOR TFT_YELLOW
#define BACKGROUND TFT_BLACK
// #define PIN_NOT_WAKE 16 // ccs811

// Анимация
float currentAngle = 30.0;
float animStep = 1.5;
bool increasing = true;
uint32_t animTimer = 0;
uint32_t dataTimer = 0;

// Данные сенсора
float temperature = 0;
float humidity = 0;
float pressure = 0;
bool bmeOK = false; // Флаг наличия датчика

uint16_t tVOC;
uint16_t CO2;

void setup() {
  Serial.begin(115200);
  // Wire.begin(D2, D1); // SDA=D2 (GPIO4), SCL=D1 (GPIO5)
  // Wire.setClock(100000); // Частота I2C 100 кГц
  Wire.begin();

  // Инициализация дисплея
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(BACKGROUND);
  // pinMode(TFT_BL, OUTPUT);
  // digitalWrite(TFT_BL, HIGH);

  // Настройка параметров датчика
  bme.setMode(NORMAL_MODE);
  bme.setFilter(4);
  bme.setHumOversampling(OVERSAMPLING_1);
  bme.setTempOversampling(OVERSAMPLING_1);
  bme.setPressOversampling(OVERSAMPLING_1);

  // Инициализация BME280 с проверкой адресов
  delay(10);
  bmeOK = bme.begin(); // По умолчанию 0x76
  if (!bmeOK) bmeOK = bme.begin(0x77); // Проверка альтернативного адреса

  if (!bmeOK) {
    showError("BME280 Error!");
    while(1); // Остановка программы при ошибке
  }
  
  // Инициализация ccs811 и вывод статуса ошибок
  CCS811Core::CCS811_Status_e returnCode = myCCS811.beginWithStatus();
  Serial.print("CCS811 begin exited with: ");
  Serial.println(myCCS811.statusString(returnCode));
  
  tft.fillScreen(BACKGROUND);
  drawStaticUI();
}

void showError(const char* msg) {
  tft.setTextColor(TFT_RED, TFT_BLACK);
  tft.setTextSize(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(msg, CENTER_X, CENTER_Y);
}

void drawStaticUI() {
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.drawString("Temp:", 60, 60);
  tft.drawString("Hum:", 60, 80);
  tft.drawString("Pres:", 60, 100);
  tft.drawString("CO2:", 60, 120);
  tft.drawString("tVOC:", 60, 140);
}

void updateSensorData() {
  if (bmeOK && (millis() - dataTimer > 2000)) {
    dataTimer = millis();
    
    // Чтение данных с проверкой
    temperature = bme.readTemperature();
    humidity = bme.readHumidity();
    pressure = bme.readPressure() / 133;//100.0F;

    // Чтение данных с ccs811
    if (myCCS811.dataAvailable()) {
      //Calling this function updates the global tVOC and eCO2 variables
      myCCS811.readAlgorithmResults();
      // printInfoSerial fetches the values of tVOC and eCO2
      // printInfoSerial();

	    tVOC = myCCS811.getTVOC();
	    CO2 = myCCS811.getCO2();
      
      //This sends the temperature data to the CCS811
      myCCS811.setEnvironmentalData(humidity, temperature);
    }

    // Обновление значений
    tft.setTextColor(TFT_PURPLE);
    tft.setTextSize(2);
    
    // Температура
    tft.fillRect(120, 60, 100, 20, BACKGROUND);
    tft.drawString(String(temperature,1) + "C", 140, 60);
    
    // Влажность
    tft.fillRect(120, 80, 100, 20, BACKGROUND);
    tft.drawString(String(humidity,1) + "%", 140, 80);
    
    // Давление
    tft.fillRect(120, 100, 100, 20, BACKGROUND);
    tft.drawString(String(pressure,0) + "hPa", 140, 100);

    // CO2
    tft.fillRect(120, 120, 100, 20, BACKGROUND);
    tft.drawString(String(CO2) + "ppm", 140, 120);

    // TVOC
    tft.fillRect(120, 140, 100, 20, BACKGROUND);
    tft.drawString(String(tVOC) + "ppb", 140, 140);
    
    // Отладка в Serial
    Serial.print("Temp: "); Serial.print(temperature);
    Serial.print(" Hum: "); Serial.print(humidity);
    Serial.print(" Press: "); Serial.println(pressure);
    Serial.print(" CO2: "); Serial.println(CO2);
    Serial.print(" tVOC: "); Serial.println(tVOC);
  }
}

void drawArc(float angle) {
  tft.drawSmoothArc(CENTER_X, CENTER_Y, 
                   ARC_RADIUS, ARC_WIDTH,
                   45, angle, 
                   ARC_COLOR, BACKGROUND, true);
}

void updateAnimation() {
  if (millis() - animTimer > 16) { // ~60 FPS
    animTimer = millis();
    
    tft.startWrite();
    //Стираем предыдущую дугу
    tft.drawSmoothArc(CENTER_X, CENTER_Y, 
                     ARC_RADIUS, ARC_WIDTH,
                     45, currentAngle, 
                     BACKGROUND, BACKGROUND, true);
    
    // Обновляем угол
    currentAngle += (increasing ? animStep : -animStep);
    
    // Проверка границ
    if (currentAngle >= 315) increasing = false;
    if (currentAngle <= 45) increasing = true;
    
    // Рисуем новую дугу
    drawArc(currentAngle);
    tft.endWrite();
  }
}

void loop() {
  updateAnimation();
  updateSensorData();
}