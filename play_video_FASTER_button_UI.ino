#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>
#include <TJpg_Decoder.h>
#include <MPU6050_tockn.h>
#include <Wire.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

TFT_eSPI tft = TFT_eSPI();   // 初始化TFT屏幕
// WiFi配置
const char* ssid ="";

const char* password="";

// API配置
const char* timeApiUrl = "https://quan.suning.com/getSysTime.do";
const char* weatherApiUrl = "https://api.openweathermap.org/data/2.5/weather?q=shanghai&appid=658767b0ae936b022f59a69f44868419&units=metric";

void connectToWiFi() {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);  
  tft.setTextSize(2);
  tft.setCursor(0, 60);
  tft.println("Connecting to WiFi...");
  
  WiFi.begin( ssid ,  password);
  
  unsigned long startAttemptTime = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 3000) {
    delay(500);
    tft.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    tft.println("\nWiFi connected");
  } else {
    tft.fillScreen(TFT_BLACK); // 清屏
    tft.setTextColor(TFT_RED, TFT_BLACK);  
    tft.setTextSize(2);
    tft.setCursor(0, 60);
    tft.println("Not WiFi connected");
  }
}
String time_string ="";
String date_string = "";
float temperature = 24.0f;
String weatherDescription = "";
void getTimeAndWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    // 获取时间
    String timePayload;
    if (httpGET(timeApiUrl, timePayload)) {
      DynamicJsonDocument timeDoc(1024);
      deserializeJson(timeDoc, timePayload);
      String dateTime = timeDoc["sysTime2"];
      time_string = dateTime.substring(11, 16); // 提取时间部分
      date_string = dateTime.substring(0, 10); // 提取时间部分

   
    }

    // 获取天气
    String weatherPayload;
    if (httpGET(weatherApiUrl, weatherPayload)) {
      DynamicJsonDocument weatherDoc(2048); // 增加文档大小
      deserializeJson(weatherDoc, weatherPayload);
      temperature = weatherDoc["main"]["temp"];
      weatherDescription = weatherDoc["weather"][0]["description"].as<String>();


    }

  } 
}


void displayTimeAndWeather(){


      // 显示时间
      // tft.fillRect(0, 0, 240, 120, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);  
      tft.setTextSize(6);
      tft.setCursor(25, 30);
      tft.println(time_string);
      tft.setTextSize(2);
      tft.setCursor(10, 100);
      tft.println(date_string);   
      // 显示天气信息
      // tft.fillRect(0, 120, 240, 160, TFT_BLACK); // 清除天气区域
      tft.setTextColor(TFT_WHITE, TFT_BLACK);  
      tft.setTextSize(2);
      tft.setCursor(10, 120);
      //tft.print("Temp: ");
      tft.print(temperature);
      tft.println(" C");
      tft.setTextSize(2);
      tft.setCursor(10, 140);
      //tft.print("Weather: ");
      tft.println(weatherDescription);


}

bool httpGET(const char* url, String &payload) {
  HTTPClient http;
  http.begin(url);
  
  
  int attempts = 0;
  while (attempts < 4) { // 重试四次
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
      payload = http.getString();
      http.end();
      return true;
    } else {
      Serial.printf("HTTP GET failed, error code: %d\n", httpResponseCode);
      delay(2000); // 等待两秒后重试
    }
    attempts++;
  }
  
  http.end();
  return false;
}

String sysPath = "/3d/0/";



#define CS 7
#define MOSI_PIN 8
#define SCK_PIN 9
#define MISO_PIN 10
// #define CS 8
// #define MOSI_PIN 9
// #define SCK_PIN 10
// #define MISO_PIN 11


#define SDA_PIN 12
#define SCL_PIN 13

MPU6050 mpu6050(Wire);
SPIClass CustomSPI;
File myFile;

uint8_t frame_0[1024 * 38] PROGMEM = {0};
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
        if (file.isDirectory() && file.name()!="System Volume Information") {
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


String readFile(fs::FS &fs, const char *path)
{
    // Serial.printf("Reading file: %s\r\n", path);
    String result = "";
    File file = fs.open(path);
    if (!file || file.isDirectory())
    {
        Serial.println("- failed to open file for reading");
        return result;
    }

    // Serial.println("- read from file:");

    while (file.available())
    {
        // Serial.write(file.read());

        result += (char)file.read();
    }

    file.close();
    return result;
}

/////////////////////////////

// === 旋转编码器与屏幕相关的引脚配置 ===
#define PIN_A 14   // A 相信号
#define PIN_B 16   // B 相信号
#define PIN_SW 15  // 按键信号


int pageIndex = 0;           // 当前页面索引变量
bool buttonPressed = false;  // 按键状态标志

// 旋转方向检测所需的变量
volatile int lastA = 0, lastB = 0;

// 创建FreeRTOS任务句柄
TaskHandle_t displayTaskHandle;
TaskHandle_t displaytimeTaskHandle;
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

/////////////////////////////////////
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
void displayTask() {


  for (;;) {



      
      if (dirList[pageIndex/2]!="System Volume Information") {


        if (buttonPressed==true){
          
          // listFile(SD, ( "/3d/"+ String(dirList[pageIndex/2])+"/"+"1").c_str(), 0);
          String info =  readFile(SD,( "/3d/"+ String(dirList[pageIndex/2])+"/info.txt").c_str());
          fileIndex = info.toInt();
          sysPath = "/3d/"+ String(dirList[pageIndex/2])+"/";
          frameIndex = 1;
          if (pageIndex/2>fileIndex) pageIndex=0;
          tft.fillScreen(TFT_BLACK);
          // tft.setCursor(0, 20);
          // tft.printf("Page: %d  ", int(pageIndex/2));



          buttonPressed = false;
        }

        if (int(pageIndex/2)==0) {

          // unsigned long  previousmillis = millis(); 
          long unsigned int premillis = millis()%100000;
          if (String(premillis).substring(0,3)=="900" && String(premillis).length()==5)
          {
        
          // tft.setCursor(20, 50);
          // tft.setTextSize(2);
          getTimeAndWeather();  
          
          // tft.printf("World");     
          }
          displayTimeAndWeather();
          
          

          tft.setCursor(20, 200);
          tft.printf(String(premillis).c_str());  
          // tft.setTextSize(2);
          // tft.printf("Hello");          



        }
  
        if (int(pageIndex/2)!=0){


          // tft.setCursor(0, 50);
          // tft.printf("info %d  ",fileIndex);          
          // tft.setCursor(0, 100);
          // tft.printf("Dir name %s:  ","3d/"+ String(dirList[pageIndex/2])+"/");
          // tft.setCursor(0, 120);
          // tft.printf("Total dir num: %d ",dirIndex );

          // for (int i=0;i<dirIndex;i++){
          //   tft.setCursor(0, 140+i*15);
          //   // tft.printf("Dir name: %s",dirList[i] );
          //   if ( String(dirList[pageIndex/2])== String(dirList[i])) tft.printf(">Dir name: %s  ",dirList[i] );
          //   else tft.printf(" Dir name: %s",dirList[i] );
          // }



          mpu6050.update();
          angleZ = mpu6050.getAngleZ();
          //angleIndex = (int)(72 * 2 * abs(atan(tan(angleZ / (73 * PI)))) / PI);
          angleIndex = (int)(36+72  * atan(tan(2*angleZ / (73 * PI)))/ PI) ;
          filename = String(sysPath) + String(angleIndex) + "/" + String(frameIndex) + ".bin";
          draw_pic_bin(filename.c_str());
          frameIndex = (frameIndex + 1) % fileIndex;


        }



      }



      // vTaskDelay(pdMS_TO_TICKS(20));  // 每200ms更新一次显示
  }




}



///////////////////////////////



void setup() {
    Serial.begin(115200);
    tft.init();
    tft.setRotation(4);
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);


    tft.fillScreen(TFT_BLACK);
    tft.setTextSize(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setCursor(0, 60);
    tft.println("Init sd...");
    CustomSPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS);
    if (!SD.begin(CS, CustomSPI, 8000000)) {
        Serial.println("SD initialization failed!");
        return;
    }
    
    listDir(SD, "/3d", 0);
    tft.setCursor(0, 60);
    tft.println("Init mpu6050...");
    Wire.begin(SDA_PIN, SCL_PIN);
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true);
    tft.setCursor(0, 60);
    tft.println("Init SIQ-02FVS3...");


    // 初始化旋转编码器引脚
    pinMode(PIN_A, INPUT_PULLUP);
    pinMode(PIN_B, INPUT_PULLUP);
    pinMode(PIN_SW, INPUT_PULLUP);

    // 注册旋转编码器与按键的中断服务
    attachInterrupt(digitalPinToInterrupt(PIN_A), handleRotation, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SW), handleButtonPress, CHANGE);
    tft.setCursor(0, 60);

    DynamicJsonDocument wifiInfoDoc(1024);
    String wifiInfoString =  readFile(SD,"/wifi.txt");
    deserializeJson(wifiInfoDoc, wifiInfoString);


    ssid = wifiInfoDoc["ssid"];
    password = wifiInfoDoc["password"];

    connectToWiFi();


    tft.fillScreen(TFT_BLACK);
    tft.println("Get time...");
    tft.fillScreen(TFT_BLACK);
    getTimeAndWeather();
    displayTimeAndWeather();
    // tft.println();
    // tft.print("-");
    // tft.print(wifiInfo[0]);
    // tft.print("-");
    // tft.println();
    // tft.print("-");
    // tft.print(wifiInfo[1]);
    // tft.print("-");




    // 增大任务堆栈大小到 4096 字节
    // xTaskCreatePinnedToCore(displayTask, "DisplayTask", 1024 * 40, NULL, 1, &displayTaskHandle, 0);
    // xTaskCreatePinnedToCore(displayTime, "DisplayTime", 1024 * 2, NULL, 1, NULL, 1);
}

void loop() {
     displayTask();
    // delay(10);  // 控制刷新率，50ms 约为 20 FPS
}
