#include <TFT_eSPI.h>
TFT_eSPI tft = TFT_eSPI();

// Настройки основной дуги
#define CENTER_X 120
#define CENTER_Y 120
#define MAIN_ARC_RADIUS 120
#define MAIN_ARC_WIDTH 113
#define MAIN_ARC_COLOR TFT_MAGENTA

// Настройки вторичной дуги
#define SECOND_ARC_RADIUS 110  // Меньший радиус для вложенности
#define SECOND_ARC_WIDTH 103
#define SECOND_ARC_COLOR TFT_CYAN
#define SECOND_ARC_START 60
#define SECOND_ARC_END 120

// Цвета
#define BACKGROUND TFT_BLACK
#define TEXT_COLOR TFT_WHITE

// Текст
const char* headers[] = {"Temp:", "Hum:", "CO2:"};
const char* values[] = {"24.4", "56", "265"};
const uint16_t colors[] = {TFT_RED, TFT_BLUE, TFT_GREEN};

// Анимация
uint32_t currentAngle = 45;
float animStep = 1.5;
bool increasing = true;
uint32_t animTimer = 0;
uint32_t pauseTimer = 0;
bool inPause = false;

// Область перерисовки
uint32_t lastAngle = 45;

void setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(BACKGROUND);
  tft.setTextDatum(MC_DATUM);
  drawStaticText();
  drawSecondaryArc(); // Статичная вторичная дуга
}

// Рисуем статический текст ----------------------------------------------------
void drawStaticText() {
  int16_t yPositions[] = {CENTER_Y - 50, CENTER_Y + 15, CENTER_Y + 15};
  int16_t xPositions[] = {CENTER_X, CENTER_X - 40, CENTER_X + 40};

  for(uint8_t i = 0; i < 3; i++) {
    // Заголовок
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(2);
    tft.drawString(headers[i], xPositions[i], yPositions[i] - 15);
    
    // Значение
    tft.setTextColor(colors[i]);
    tft.setTextSize(3);
    tft.drawString(values[i], xPositions[i], yPositions[i] + 15);
  }
}

// Рисуем вторичную дугу -------------------------------------------------------
void drawSecondaryArc() {
  tft.drawSmoothArc(
    CENTER_X, CENTER_Y,
    SECOND_ARC_RADIUS, SECOND_ARC_WIDTH,
    SECOND_ARC_START, SECOND_ARC_END,
    SECOND_ARC_COLOR, BACKGROUND, true
  );
}

// Обновление анимации ---------------------------------------------------------
void updateArcAnimation() {
  if(!inPause) {
    currentAngle += increasing ? animStep : -animStep;
    
    if(currentAngle >= 315) {
      currentAngle = 315;
      inPause = true;
      pauseTimer = millis();
    }
    else if(currentAngle <= 45) {
      currentAngle = 45;
      increasing = true;
      inPause = true;
      pauseTimer = millis();
    }
  }
  else if(millis() - pauseTimer >= 500) {
    inPause = false;
    increasing = !increasing;
  }
}

// Основной цикл ---------------------------------------------------------------
void loop() {
  if(millis() - animTimer > 16) {
    animTimer = millis();
    
    updateArcAnimation();
    
    tft.startWrite();
    
    // 1. Стираем предыдущую дугу (только изменившийся сегмент)
    uint32_t eraseStart = increasing ? lastAngle : currentAngle;
    uint32_t eraseEnd = increasing ? currentAngle : lastAngle;
    
    tft.drawSmoothArc(
      CENTER_X, CENTER_Y,
      MAIN_ARC_RADIUS, MAIN_ARC_WIDTH,
      eraseStart, eraseEnd,
      BACKGROUND, BACKGROUND, true
    );
    
    // 2. Рисуем новую дугу
    tft.drawSmoothArc(
      CENTER_X, CENTER_Y,
      MAIN_ARC_RADIUS, MAIN_ARC_WIDTH,
      45, currentAngle,
      MAIN_ARC_COLOR, BACKGROUND, true
    );
    
    // 3. Восстанавливаем пересечение со вторичной дугой
    if(currentAngle > SECOND_ARC_START && lastAngle < SECOND_ARC_END) {
      drawSecondaryArc();
    }
    
    tft.endWrite();
    
    lastAngle = currentAngle;
  }
}