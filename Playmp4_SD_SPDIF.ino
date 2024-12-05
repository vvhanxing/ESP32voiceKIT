#include <Arduino.h>
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

TFT_eSPI tft = TFT_eSPI();
AudioFileSourceSD *source = NULL;
AudioOutputI2S *output = NULL;
//AudioGeneratorFLAC *decoder = NULL;
AudioGeneratorWAV *decoder = NULL;
File picFile;
File myFile;
uint8_t frame_0[1024 * 32] PROGMEM = {0};
size_t fileSize = sizeof(frame_0);

File pic_dir;
File wav_dir;

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap) {
    if (y >= tft.height()) return false;
    tft.pushImage(x, y, w, h, bitmap);
    return true;
}

void draw_pic_bin_task(void *pvParameters) {
    while (1) {
        picFile = pic_dir.openNextFile();
        if (picFile) {
            if (String(picFile.name()).endsWith(".bin")) {
                size_t bytesRead = picFile.readBytes((char *)frame_0, fileSize); // 使用 readBytes 提高读取效率
                if (bytesRead > 0) {
                    TJpgDec.drawJpg(0, 0, frame_0, bytesRead);
                } else {
                    Serial.printf("Error reading binary file: %s\n", picFile.name());
                }
                picFile.close();
            }
        } else {
            pic_dir.rewindDirectory(); // Rewind directory for loop playback
        }
        vTaskDelay(5 / portTICK_PERIOD_MS); // 减小延迟提升帧率
    }
}

void playwav_task(void *pvParameters) {
    while (1) {
        if (decoder && decoder->isRunning()) {
            if (!decoder->loop()) decoder->stop();
        } else {
            File file = wav_dir.openNextFile();
            if (file) {
                if (String(file.name()).endsWith(".wav")) {
                    source->close();
                    if (source->open(("/" + String(file.name())).c_str())) {
                        Serial.printf("Playing '%s' from SD card...\n", file.name());
                        decoder->begin(source, output);
                    } else {
                        Serial.printf("Error opening '%s'\n", file.name());
                    }
                }
                file.close(); // 确保关闭文件释放资源
            } else {
                wav_dir.rewindDirectory(); // Rewind directory for loop playback
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS); // 防止任务抢占过多CPU
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);


    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    TJpgDec.setJpgScale(1);
    TJpgDec.setSwapBytes(true);
    TJpgDec.setCallback(tft_output);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(0, 60);
    tft.println("Say something");

    audioLogger = &Serial;
    source = new AudioFileSourceSD();
    output = new AudioOutputI2S();
    decoder = new AudioGeneratorWAV();

    output->SetGain(0.1);
    output->SetPinout(40, 42, 17);

    CustomSPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS);

    #if defined(ESP8266)
        SD.begin(SS, SPI_SPEED);
    #else
        if (!SD.begin(CS, CustomSPI, 20000000)) {
            Serial.println("SD card initialization failed!");
        } else {
            Serial.println("SD card initialized successfully.");
        }
    #endif

    pic_dir = SD.open("/output");
    wav_dir = SD.open("/");

    // Create tasks for video and audio
    xTaskCreatePinnedToCore(playwav_task, "PlayWAV", 1024*4, NULL, 1, NULL, 1);
    xTaskCreatePinnedToCore(draw_pic_bin_task, "DrawPic", 1024*8, NULL, 2, NULL, 0);
}

void loop() {
    // Loop is intentionally empty as tasks handle playback and video frame rendering
}
