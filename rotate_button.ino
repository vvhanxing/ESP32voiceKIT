#include <TFT_eSPI.h> // TFT驱动库
#include <Arduino.h>

// === 旋转编码器与屏幕相关的引脚配置 ===
#define PIN_A 14  // A 相信号
#define PIN_B 16  // B 相信号
#define PIN_SW 15 // 按键信号




TFT_eSPI tft = TFT_eSPI(); // 初始化TFT屏幕
int pageIndex = 0;          // 当前页面索引变量
bool buttonPressed = false; // 按键状态标志

// 旋转方向检测所需的变量
volatile int lastA = 0, lastB = 0;

// 创建FreeRTOS任务句柄
TaskHandle_t displayTaskHandle;

void IRAM_ATTR handleRotation() {
  int A = digitalRead(PIN_A);
  int B = digitalRead(PIN_B);

  if (A != lastA) { // 检测A相的变化
    if (A == B) {
      pageIndex++; // 顺时针
    } else {
      pageIndex--; // 逆时针
    }
  }
  lastA = A;
  lastB = B;
}

// 按键中断处理程序
void IRAM_ATTR handleButtonPress() {
  static unsigned long pressStartTime = 0;

  if (digitalRead(PIN_SW) == LOW) { // 按键按下时记录时间
    pressStartTime = millis();
  } else { // 按键松开时判断按下时间
    unsigned long pressDuration = millis() - pressStartTime;
    if (pressDuration < 500) {
      pageIndex = 1; // 短按：将 pageIndex 设置为 1
    } else {
      pageIndex = 0; // 长按：将 pageIndex 设置为 0
    }
  }
}

// FreeRTOS任务：更新屏幕显示
void displayTask(void *parameter) {
  for (;;) {
    tft.fillScreen(TFT_BLACK);         // 清空屏幕
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextSize(3);

    // 在屏幕中央显示pageIndex的值
    tft.setCursor(40, 60);
    tft.printf("Page: %d", pageIndex);

    vTaskDelay(pdMS_TO_TICKS(200)); // 每200ms更新一次显示
  }
}

void setup() {
  // 初始化旋转编码器引脚
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);

  // 注册旋转编码器与按键的中断服务
  attachInterrupt(digitalPinToInterrupt(PIN_A), handleRotation, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_SW), handleButtonPress, CHANGE);

  // 初始化TFT屏幕
  tft.init();
  tft.setRotation(1);

  // 增大任务堆栈大小到 4096 字节
  xTaskCreatePinnedToCore(displayTask, "DisplayTask", 4096, NULL, 1, &displayTaskHandle, 1);
}

void loop() {
  // 主循环无需逻辑，由中断与FreeRTOS任务处理
}
