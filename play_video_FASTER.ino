#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <MPU6050_tockn.h>
#include <Wire.h>

#define CS 7
#define MOSI_PIN 8
#define SCK_PIN 9
#define MISO_PIN 10
#define SDA_PIN 12
#define SCL_PIN 13

MPU6050 mpu6050(Wire);
TFT_eSPI tft = TFT_eSPI();
SPIClass CustomSPI;
File myFile;

uint8_t frame_0[1024 * 40] PROGMEM = {0};
size_t fileSize = sizeof(frame_0);
int frameIndex = 1, angleIndex = 1;
float angleZ = 0;

const char *sysPath = "/move2/";
String fileList[512] = {""};
int fileIndex = 0;

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
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
    
    listDir(SD, "/move2/1", 0);
    tft.setCursor(0, 50);
    tft.println("Init mpu6050...");
    Wire.begin(SDA_PIN, SCL_PIN);
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true);


}

void loop() {
    mpu6050.update();
    angleZ = mpu6050.getAngleZ();
    //angleIndex = (int)(72 * 2 * abs(atan(tan(angleZ / (73 * PI)))) / PI);
    angleIndex = (int)(36+72  * atan(tan(-2*angleZ / (73 * PI)))/ PI) ;

    String filename = String(sysPath) + String(angleIndex) + "/" + String(frameIndex) + ".bin";
    draw_pic_bin(filename.c_str());

    frameIndex = (frameIndex + 1) % fileIndex;
    delay(10);  // 控制刷新率，50ms 约为 20 FPS
}
