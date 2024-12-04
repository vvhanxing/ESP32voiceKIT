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

File myFile;
uint8_t frame_0[1024 * 38] PROGMEM = {0};
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
        File picFile = pic_dir.openNextFile();
        if (picFile) {
            if (String(picFile.name()).endsWith(".bin")) {
                myFile = SD.open(("/output/" + String(picFile.name())).c_str(), FILE_READ);
                if (myFile) {
                    myFile.read(frame_0, fileSize);
                    TJpgDec.drawJpg(0, 0, frame_0, fileSize);
                    myFile.close();
                } else {
                    Serial.println("Error opening binary file:");
                    Serial.println(picFile.name());
                }
            }
        } else {
            pic_dir.rewindDirectory(); // Rewind directory for loop playback
        }
        delay(100); // Delay to control frame rate
    }
}

void playwav_task(void *pvParameters) {
    while (1) {
        if ((decoder) && (decoder->isRunning())) {
            if (!decoder->loop()) decoder->stop();
        } else {
            File file = wav_dir.openNextFile();
            if (file) {
                if (String(file.name()).endsWith(".wav")) {
                    source->close();
                    if (source->open(("/" + String(file.name())).c_str())) {
                        Serial.printf_P(PSTR("Playing '%s' from SD card...\n"), file.name());
                        decoder->begin(source, output);
                    } else {
                        Serial.printf_P(PSTR("Error opening '%s'\n"), file.name());
                    }
                }
            } else {
                wav_dir.rewindDirectory(); // Rewind directory for loop playback
            }
        }
        delay(10); // Small delay to prevent task starvation
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
        if (!SD.begin(CS, CustomSPI, 8000000)) {
            Serial.println("SD card initialization failed!");
        } else {
            Serial.println("SD card initialized successfully.");
        }
    #endif

    pic_dir = SD.open("/output");
    wav_dir = SD.open("/");

    // Create tasks for video and audio
    xTaskCreatePinnedToCore(playwav_task, "PlayWAV", 4096, NULL, 0, NULL, 0);
    xTaskCreatePinnedToCore(draw_pic_bin_task, "DrawPic", 4096, NULL, 1, NULL, 1);
}

void loop() {
    // Loop is intentionally empty as tasks handle playback and video frame rendering
}
