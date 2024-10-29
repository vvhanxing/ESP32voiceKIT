#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <TJpg_Decoder.h>
#include <MPU6050_tockn.h>
#include <Wire.h>


String sysPath = "/2/";



#define CS 7
#define MOSI_PIN 8
#define SCK_PIN 9
#define MISO_PIN 10
#define SDA_PIN 12
#define SCL_PIN 13

MPU6050 mpu6050(Wire);
SPIClass CustomSPI;
File myFile;

uint8_t frame_0[1024 * 36] PROGMEM = {0};
size_t fileSize = sizeof(frame_0);
int frameIndex = 1, angleIndex = 1;
float angleZ = 0;


String fileList[512] = {""};
String dirList[512] = {""};
int fileIndex = 0;
int dirIndex = 0;

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    dirIndex = 0;
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory() || file.name()!="System Volume Information") {
            dirList[dirIndex++] = file.name();
          
        }
        file = root.openNextFile();
    }
}


void listFile(fs::FS &fs, const char *dirname, uint8_t levels) {
    fileIndex = 0;
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            fileList[fileIndex++] = file.name();
          
        }
        file = root.openNextFile();
    }
}


/////////////////////////////

// === 旋转编码器与屏幕相关的引脚配置 ===
#define PIN_A 14   // A 相信号
#define PIN_B 16   // B 相信号
#define PIN_SW 15  // 按键信号

TFT_eSPI tft = TFT_eSPI();   // 初始化TFT屏幕
int pageIndex = 0;           // 当前页面索引变量
bool buttonPressed = false;  // 按键状态标志

// 旋转方向检测所需的变量
volatile int lastA = 0, lastB = 0;

// 创建FreeRTOS任务句柄
TaskHandle_t displayTaskHandle;

void IRAM_ATTR handleRotation() {
  int A = digitalRead(PIN_A);
  int B = digitalRead(PIN_B);

  if (A != lastA) {  // 检测A相的变化
    if (A == B) {
      pageIndex++;  // 顺时针
      buttonPressed = true;
      
   
    } else {
      pageIndex--;  // 逆时针
      if (pageIndex<0) pageIndex = 0;
      buttonPressed = true;

    }

  }
  lastA = A;
  lastB = B;
}

// 按键中断处理程序
void IRAM_ATTR handleButtonPress() {
  static unsigned long pressStartTime = 0;

  if (digitalRead(PIN_SW) == LOW) {  // 按键按下时记录时间
    pressStartTime = millis();
  } else {  // 按键松开时判断按下时间
    unsigned long pressDuration = millis() - pressStartTime;
    if (pressDuration < 500) {
      pageIndex = 2;  // 短按：将 pageIndex 设置为 1
      buttonPressed = true;
      
    } else {
      pageIndex = 0;  // 长按：将 pageIndex 设置为 0
      buttonPressed = true;
    }
  }
}
////////////////////////////////////////

void draw_pic_bin(const char *pic_name) {
    myFile = SD.open(pic_name, FILE_READ);
    if (myFile) {
        myFile.read(frame_0, fileSize);
        TJpgDec.drawJpg(0, 0, frame_0, fileSize);
        myFile.close();
    } else {
        Serial.println("Error opening binary file:");
        Serial.println(pic_name);
    }
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) return 0;
    tft.pushImage(x, y, w, h, bitmap);
    return 1;
}

String filename = "";
// FreeRTOS任务：更新屏幕显示
void displayTask(void *parameter) {
  for (;;) {



      
      if (dirList[pageIndex/2]!="System Volume Information") {


        if (buttonPressed==true){
          
          listFile(SD, ( "/"+ String(dirList[pageIndex/2])+"/"+"1").c_str(), 0);
          sysPath = "/"+ String(dirList[pageIndex/2])+"/";
          buttonPressed = false;
        }

        mpu6050.update();
        angleZ = mpu6050.getAngleZ();
        //angleIndex = (int)(72 * 2 * abs(atan(tan(angleZ / (73 * PI)))) / PI);
        angleIndex = (int)(36+72  * atan(tan(-2*angleZ / (73 * PI)))/ PI) ;
        filename = String(sysPath) + String(angleIndex) + "/" + String(frameIndex) + ".bin";
        draw_pic_bin(filename.c_str());
        frameIndex = (frameIndex + 1) % fileIndex;
      }
      else{

        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(3);

        // 在屏幕中央显示pageIndex的值
        tft.setCursor(40, 80);
        tft.printf("Page: %d  ", int(pageIndex/2));
        tft.setCursor(40, 100);
        tft.printf("%s  ",dirList[pageIndex/2]);

      }



      vTaskDelay(pdMS_TO_TICKS(20));  // 每200ms更新一次显示
  }
}
///////////////////////////////



void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(0);
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);


    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 50);
    tft.println("Init sd...");
    CustomSPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS);
    if (!SD.begin(CS, CustomSPI, 8000000)) {
        Serial.println("SD initialization failed!");
        return;
    }
    
    listDir(SD, "/", 0);
    tft.setCursor(0, 50);
    tft.println("Init mpu6050...");
    Wire.begin(SDA_PIN, SCL_PIN);
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true);


    // 初始化旋转编码器引脚
    pinMode(PIN_A, INPUT_PULLUP);
    pinMode(PIN_B, INPUT_PULLUP);
    pinMode(PIN_SW, INPUT_PULLUP);

    // 注册旋转编码器与按键的中断服务
    attachInterrupt(digitalPinToInterrupt(PIN_A), handleRotation, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SW), handleButtonPress, CHANGE);


    // 增大任务堆栈大小到 4096 字节
    xTaskCreatePinnedToCore(displayTask, "DisplayTask", 1024 * 40, NULL, 1, &displayTaskHandle, 0);
}

void loop() {

    // delay(10);  // 控制刷新率，50ms 约为 20 FPS
}
