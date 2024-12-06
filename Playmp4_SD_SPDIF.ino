// Example for library:
// https://github.com/Bodmer/TJpg_Decoder

// This example renders a Jpeg file that is stored in an array within Flash (program) memory
// see panda.h tab.  The panda image file being ~13Kbytes.
#include <Arduino.h>
#include "AudioFileSourceSD.h"
#include "AudioOutputSPDIF.h"
// #include "AudioGeneratorFLAC.h"
#include "AudioGeneratorWAV.h"
#include "AudioOutputI2S.h"




#define USE_DMA
// Include the array
#include "panda.h"
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



#include "FS.h"
#include "SD.h"
#include "SPI.h"


SPIClass CustomSPI;
#define CS 8
#define MOSI_PIN 9
#define SCK_PIN 10
#define MISO_PIN 11
#define SPI_SPEED SD_SCK_MHZ(40)


AudioFileSourceSD *source = NULL;
AudioOutputI2S *output = NULL;
// AudioGeneratorFLAC *decoder = NULL;
AudioGeneratorWAV *decoder = NULL;
File picFile;
File wevFile;
uint8_t frame_0[1024 * 32] PROGMEM = {0};
size_t fileSize = sizeof(frame_0);

File pic_dir;
File wav_dir;



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


void setup()
{
  Serial.begin(115200);
  Serial.println("\n\n Testing TJpg_Decoder library");

  // Initialise the TFT
  tft.begin();
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);

#ifdef USE_DMA
  tft.initDMA(); // To use SPI DMA you must call initDMA() to setup the DMA engine
#else
  tft.init();
  
#endif

  // The jpeg image can be scaled down by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The colour byte order can be swapped by the decoder
  // using TJpgDec.setSwapBytes(true); or by the TFT_eSPI library:
  tft.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output_dma);






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

    pic_dir = SD.open("/output/frames");
    wav_dir = SD.open("/output");







    audioLogger = &Serial;
    source = new AudioFileSourceSD();
    output = new AudioOutputI2S();
    decoder = new AudioGeneratorWAV();

    output->SetGain(0.5);
    output->SetPinout(40, 42, 17);


    // Create tasks for video and audio
    xTaskCreatePinnedToCore(playwav_task, "PlayWAV", 1024 * 4, NULL,1, NULL, 0);
    xTaskCreatePinnedToCore(draw_pic_bin_task, "DrawPic", 1024 * 4, NULL, 2, NULL, 1);

}

void loop()
{


}
