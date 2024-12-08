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
// Include the array

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



AudioFileSourceSD *source = NULL;
AudioOutputI2S *output = NULL;
// AudioGeneratorFLAC *decoder = NULL;
AudioGeneratorWAV *decoder = NULL;
File picFile;
File wevFile;
uint8_t frame_0[1024 * 36] PROGMEM = {0};
size_t fileSize = sizeof(frame_0);

File pic_dir;
File wav_dir;


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



bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap); // 使用 pushImage 而非 pushImageDMA
    return true;
}
void draw_pic_bin_task(void *pvParameters) {
    while (1) {
        picFile = pic_dir.openNextFile();
        if (picFile) {
            if (String(picFile.name()).endsWith(".bin")) {
                size_t bytesRead = picFile.readBytes((char *)frame_0, fileSize);
                if (bytesRead > 0) {
                    tft.startWrite();
                    TJpgDec.drawJpg(0, 0, frame_0, bytesRead);
                    tft.endWrite();
                } else {
                    Serial.printf("Error reading binary file: %s\n", picFile.name());
                }
                picFile.close();
            }
        } else {
            pic_dir.rewindDirectory(); // 循环播放
        }
        vTaskDelay(2 / portTICK_PERIOD_MS);
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


/////////////////////////////////////

int frameCount = 1;
int detailLevel = 72;
String sysPath = "";
int frameIndex = 1;

int angleZIndex = 1;
float angleZ = 0;
File myFile;

String filename = "";


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




int clickA_push_count = 0;
int clickA_once_count = 0;
int animater_clip_index = 0;
String animater_clip_list[2] = {"/3d/9/","/3d/10/"};
void draw_AR_task(void *pvParameters) {
    while (1) {
    mpu6050.update();

    angleZ = mpu6050.getAngleZ();
    angleZIndex = (int)(detailLevel*0.5+ detailLevel*atan(tan(2*angleZ / ((detailLevel+2) * PI)))/ PI) ;



  if(clickA()){
      clickA_push_count = clickA_push_count+1;
      if (clickA_push_count==2){ //短按
        clickA_once_count +=1;
          
        }
      if (clickA_push_count==10){//长按
        clickA_once_count -=2;
        }
       Serial.println(clickA_push_count);
       
    }
    else{
      clickA_push_count = 1;
      tft.setCursor(120, 20); 
      // tft.print(clickA_once_count);    // Print the font name onto the TFT screen  
      if (clickA_once_count>1) clickA_once_count=0;
      animater_clip_index = clickA_once_count;   

      
      sysPath = animater_clip_list[animater_clip_index];
      String infoPayload = readFile(SD, (sysPath+"info.txt").c_str());
      DynamicJsonDocument infoDoc(1024);  // Increase document size
      deserializeJson(infoDoc, infoPayload);
      frameCount = infoDoc["frameCount"];
      detailLevel = infoDoc["detailLevel"];    
      // Serial.println(sysPath);
        
    }
    


    filename = String(sysPath) + String(angleZIndex) + "/" + String(frameIndex) + ".bin";
    draw_pic_bin(filename.c_str());
    frameIndex = (frameIndex + 1) % frameCount;
    if (sysPath=="/3d/10/"  && frameIndex==frameCount-1)
    {
      clickA_once_count = 0;
      
    }


    Serial.println(frameIndex);
    Serial.println(frameCount);
    Serial.println(sysPath);







    }
    vTaskDelay(2 / portTICK_PERIOD_MS);


}

////////////////////////////////////////


void playwav_task(void *pvParameters) {
    while (1) {
        if (decoder && decoder->isRunning()) {
            if (!decoder->loop()) decoder->stop();
        } else {
            wevFile = wav_dir.openNextFile();
            if (wevFile) {
                if (String(wevFile.name()).endsWith(".wav")) {
                    source->close();
                    if (source->open(("/output/" + String(wevFile.name())).c_str())) {
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
////////////////////////////////////////

bool clickA(){

   const int touchPin = 7; // 使用 T0 获取数据
   const int threshold = 20000;
   // variable for storing the touch pin value 
   int touchValue;
  // read the state of the pushbutton value:
  touchValue = touchRead(touchPin);
  // Serial.println(touchValue);
  //Serial.print("\n");

  // check if the touchValue is below the threshold
  // if it is, set ledPin to HIGH
  if(touchValue > threshold){
    // turn LED on
    //digitalWrite(ledPin, HIGH);
     //Serial.println("有触控，灯亮");
    return true;
  }
  else{
    // turn LED off
    //digitalWrite(ledPin, LOW);
    //Serial.println(" - LED off");
    return false;
  }
  //delay(500);  
  }

  //////////



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

    pic_dir = SD.open("/output/frames");
    wav_dir = SD.open("/output");



    String filename = "";
    sysPath = "/3d/9/";    
    String infoPayload = readFile(SD, (sysPath+"info.txt").c_str());
    DynamicJsonDocument infoDoc(1024);  // Increase document size
    deserializeJson(infoDoc, infoPayload);
    frameCount = infoDoc["frameCount"];
    detailLevel = infoDoc["detailLevel"];

    frameIndex = 1;
  




    // Create tasks for video and audio
    xTaskCreatePinnedToCore(playwav_task, "PlayWAV", 1024 * 4, NULL, 1, NULL, 0);
    // xTaskCreatePinnedToCore(draw_pic_bin_task, "DrawPic", 1024 * 4, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(draw_AR_task, "DrawAR", 1024 * 4, NULL, 2, NULL, 1);
}

void loop() {
    // Loop intentionally empty as tasks handle playback and video frame rendering
}
