#include <Arduino.h>
#include <ArduinoJson.h>

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include "AudioFileSourceSD.h"
#include "AudioOutputSPDIF.h"
// #include "AudioGeneratorFLAC.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"

SPIClass CustomSPI;
#define CS 8
#define MOSI_PIN 9
#define SCK_PIN 10
#define MISO_PIN 11
#define SPI_SPEED SD_SCK_MHZ(40)

#include <Wire.h>
#include <MPU6050_tockn.h>
#define SDA_PIN 12
#define SCL_PIN 13
MPU6050 mpu6050(Wire);

#define USE_DMA

// Include the jpeg decoder library
#include <TJpg_Decoder.h>
#ifdef USE_DMA
  uint16_t  dmaBuffer1[16*16]; // Toggle buffer for 16*16 MCU block, 512bytes
  uint16_t  dmaBuffer2[16*16]; // Toggle buffer for 16*16 MCU block, 512bytes
  uint16_t* dmaBufferPtr = dmaBuffer1;
  bool dmaBufferSel = 0;
#endif
// Include the TFT library https://github.com/Bodmer/TFT_eSPI
#include "SPI.h"
#include <TFT_eSPI.h>              // Hardware-specific library
TFT_eSPI tft = TFT_eSPI();         // Invoke custom library

// This next function will be called during decoding of the jpeg file to render each
// 16x16 or 8x8 image tile (Minimum Coding Unit) to the TFT.
bool tft_output_dma(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // STM32F767 processor takes 43ms just to decode (and not draw) jpeg (-Os compile option)
  // Total time to decode and also draw to TFT:
  // SPI 54MHz=71ms, with DMA 50ms, 71-43 = 28ms spent drawing, so DMA is complete before next MCU block is ready
  // Apparent performance benefit of DMA = 71/50 = 42%, 50 - 43 = 7ms lost elsewhere
  // SPI 27MHz=95ms, with DMA 52ms. 95-43 = 52ms spent drawing, so DMA is *just* complete before next MCU block is ready!
  // Apparent performance benefit of DMA = 95/52 = 83%, 52 - 43 = 9ms lost elsewhere
#ifdef USE_DMA
  // Double buffering is used, the bitmap is copied to the buffer by pushImageDMA() the
  // bitmap can then be updated by the jpeg decoder while DMA is in progress
  if (dmaBufferSel) dmaBufferPtr = dmaBuffer2;
  else dmaBufferPtr = dmaBuffer1;
  dmaBufferSel = !dmaBufferSel; // Toggle buffer selection
  //  pushImageDMA() will clip the image block at screen boundaries before initiating DMA
  tft.pushImageDMA(x, y, w, h, bitmap, dmaBufferPtr); // Initiate DMA - blocking only if last DMA is not complete
  // tft.dmaWait();  
  // The DMA transfer of image block to the TFT is now in progress...
#else
  // Non-DMA blocking alternative
  tft.pushImage(x, y, w, h, bitmap);  // Blocking, so only returns when image block is drawn
#endif
  // Return 1 to decode next block.
  return 1;
}

AudioFileSourceSD *source = NULL;
AudioOutputI2S *output = NULL;
// AudioGeneratorFLAC *decoder = NULL;
AudioGeneratorWAV *decoder = NULL;
File picFile;
File wav_dir;
File wevFile;
uint8_t frame_0[1024 * 36] PROGMEM = {0};
size_t fileSize = sizeof(frame_0);

int frameCount = 1;
int detailLevel = 72;
String sysPath = "";
int frameIndex = 1;

int angleZIndex = 1;
float angleZ = 0;
File myFile;

String filename = "";

String animater_clip_list[12] = {"/3d/9", "/3d/10", "/3d/9"};


int dirIndex = 0;

void listDir(fs::FS &fs, const char *dirname, uint8_t levels) {
    dirIndex = 0;
    File root = fs.open(dirname);
    if (!root || !root.isDirectory()) return;
    File file = root.openNextFile();
    while (file) {
        if (file.isDirectory() && file.name()!="System Volume Information") {
            animater_clip_list[dirIndex++] =String(dirname)+ "/"+file.name();
          
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


int animater_clip_index = 0;

void stopAudio() {
    if (decoder && decoder->isRunning()) {
        decoder->stop();  // Stop currently playing audio
        delay(500);  // Ensure the audio stops properly
    }
}

void startNewAudio(const String &audioPath) {
    source->close();
    if (source->open(audioPath.c_str())) {
        Serial.printf("Playing '%s' from SD card...\n", audioPath.c_str());
        decoder->begin(source, output);
    } else {
        Serial.printf("Error opening '%s'\n", audioPath.c_str());
    }
}

void draw_pic_bin(const char *pic_name) {
    picFile = SD.open(pic_name, FILE_READ);
    if (picFile) {
        size_t bytesRead = picFile.readBytes((char *)frame_0, fileSize);
        if (bytesRead > 0) {
            tft.startWrite();
            TJpgDec.drawJpg(0, 0, frame_0, bytesRead);
            tft.endWrite();
        } else {
            Serial.printf("Error reading binary file: %s\n", picFile.name());
        }
        picFile.close();
    } else {
        Serial.println("Error opening binary file:");
        Serial.println(pic_name);
    }
}

bool clickA() {
    const int touchPin = 7; // Using T0 to get data
    const int threshold = 20000;
    int touchValue = touchRead(touchPin);
    return touchValue > threshold;
}

void draw_AR_task(void *pvParameters) {
    while (1) {
        mpu6050.update();
        angleZ = mpu6050.getAngleZ();
        angleZIndex = (int)(detailLevel * 0.5 + detailLevel * atan(tan(2 * angleZ / ((detailLevel + 2) * PI))) / PI);
        filename = String(sysPath) + "/" + String(angleZIndex) + "/" + String(frameIndex) + ".bin";
        draw_pic_bin(filename.c_str());
        frameIndex = (frameIndex + 1) % frameCount;
            // for (int i=0;i<dirIndex;i++){
            //   tft.setCursor(0, 140+i*15);
        
            //   tft.printf(" Dir name: %s",animater_clip_list[i] );
            // }



        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}

void playwav_task(void *pvParameters) {
    while (1) {
        if (decoder && decoder->isRunning()) {
            if (!decoder->loop()) decoder->stop();
        } else {
            wevFile = wav_dir.openNextFile();
            if (wevFile) {
                if (String(wevFile.name()).endsWith(".wav")) {
                    source->close();
                    if (source->open((sysPath + "/" + String(wevFile.name())).c_str())) {
                        Serial.printf("Playing '%s' from SD card...\n", wevFile.name());
                        decoder->begin(source, output);
                    } else {
                        Serial.printf("Error opening '%s'\n", wevFile.name());
                    }
                }
                wevFile.close();
            } else {
                wav_dir.rewindDirectory();
            }
        }
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    tft.begin();
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.fillScreen(TFT_BLACK);

#ifdef USE_DMA
    tft.initDMA(); // To use SPI DMA you must call initDMA() to setup the DMA engine
#else
    tft.init();
#endif
    tft.setRotation(4);
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true); // 必须启用以匹配屏幕颜色顺序
    TJpgDec.setCallback(tft_output_dma);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 60);
    tft.println("init mpu6050...");

    Wire.begin(SDA_PIN, SCL_PIN);
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true);

    tft.println("init audio...  ");
    audioLogger = &Serial;
    source = new AudioFileSourceSD();
    output = new AudioOutputI2S();
    decoder = new AudioGeneratorWAV();
    output->SetGain(0.5);
    output->SetPinout(40, 42, 17);
    tft.println("init SD...      ");
    CustomSPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS);

#if defined(ESP8266)
    SD.begin(SS, SPI_SPEED);
#else
    if (!SD.begin(CS, CustomSPI, 40000000)) {
        Serial.println("SD card initialization failed!");
    } else {
        Serial.println("SD card initialized successfully.");
    }
#endif

    tft.fillScreen(TFT_BLACK);

    listDir(SD, "/3d", 0);



    animater_clip_index = 1;
    sysPath = animater_clip_list[animater_clip_index];
    String infoPayload = readFile(SD, (sysPath + "/" + "info.txt").c_str());
    DynamicJsonDocument infoDoc(1024);  // Increase document size
    deserializeJson(infoDoc, infoPayload);
    frameCount = infoDoc["frameCount"];
    detailLevel = infoDoc["detailLevel"];
    frameIndex = 1;

    wav_dir = SD.open(animater_clip_list[animater_clip_index]);

    // Create tasks for video and audio
    xTaskCreatePinnedToCore(playwav_task, "PlayWAV", 1024 * 4, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(draw_AR_task, "DrawAR", 1024 * 4, NULL, 2, NULL, 1);
}

void loop() {
    // If the touch pin is touched, stop the current audio and switch to new audio and video path
    if (clickA()) {
        stopAudio(); // Stop the current audio


        
        animater_clip_index = (animater_clip_index + 1); // Cycle through video clips
        if (animater_clip_index>=dirIndex) animater_clip_index = 0;
        sysPath = animater_clip_list[animater_clip_index];

        String infoPayload = readFile(SD, (sysPath + "/" + "info.txt").c_str());
        DynamicJsonDocument infoDoc(1024);  // Increase document size
        deserializeJson(infoDoc, infoPayload);
        frameCount = infoDoc["frameCount"];
        detailLevel = infoDoc["detailLevel"];

        // String audioPath = sysPath + "/audio.wav";  // Define your audio path
        // startNewAudio(audioPath);  // Start new audio
    }
    delay(100);  // Prevent too frequent checks
}
