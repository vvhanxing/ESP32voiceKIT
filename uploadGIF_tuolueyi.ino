#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <string.h>
#include <uri/UriBraces.h>
#include <uri/UriRegex.h>
#include "Base64.h"

#include "arduino_base64.hpp"
// #include "Free_Fonts.h"

#include "FS.h"
#include <LittleFS.h>

/* You only need to format LittleFS the first time you run a
   test or else use the LITTLEFS plugin to create a partition
   https://github.com/lorol/arduino-esp32littlefs-plugin

   If you test two partitions, you need to use a custom
   partition.csv file, see in the sketch folder */

// #define TWOPART 
#include <MPU6050_tockn.h>
#include <Wire.h>
MPU6050 mpu6050(Wire);
#define FORMAT_LITTLEFS_IF_FAILED true
#define SCL_PIN 12
#define SDA_PIN 13


const char *ssid = "HUAWEI P50 Pro"; // Enter your SSID here
const char *password = "12345678";   // Enter your Password here

// http://192.168.43.216

#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI(); // Invoke library, pins defined in User_Setup.h

////////////////////////////////////////////////////////////////

// JPEG decoder library
// Include the jpeg decoder library
#include <TJpg_Decoder.h>
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *bitmap)
{
    // Stop further decoding as image is running off bottom of screen
    if (y >= tft.height())
        return 0;

    // This function will clip the image block rendering automatically at the TFT boundaries
    tft.pushImage(x, y, w, h, bitmap);

    // This might work instead if you adapt the sketch to use the Adafruit_GFX library
    // tft.drawRGBBitmap(x, y, bitmap, w, h);

    // Return 1 to decode next block
    return 1;
}

#include "mainPageHtml.h"

int *drawArrayJpeg(const uint8_t arrayname[], uint32_t array_size, int xpos, int ypos);
void renderJPEG(int xpos, int ypos);

String urlInfo = "";
String info = "hello vivi";
String info_list[] = {info, info};

///////////////////
void screenInfo(String info, int posx, int posy, int font)
{
    // 设置起始坐标(20, 10),4 号字体
    if (info_list[0] != info_list[1])
    {
        tft.fillScreen(TFT_WHITE);
    }
    tft.setCursor(posx, posy);
    // 设置文本颜色为白色,黑色文本背景
    tft.setTextFont(font);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // 设置显示的文字,注意这里有个换行符 \n 产生的效果
    tft.println(info);
}

/////////////////
WebServer server(80);
void initWIFI()
{

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    Serial.println("123");
    // Wait for connection
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    urlInfo = WiFi.localIP().toString();
    if (MDNS.begin("esp32"))
    {
        Serial.println("MDNS responder started");
    }
}

void MainPage()
{
    server.send(200, "text/html", mainPageHtmlString(urlInfo));
    Serial.println("MainPage");
}

void putInfo()
{
    //  info = server.arg("info") ;
    //  Serial.println(info);
    //
    //
    //  Serial.println(server.arg(0));
    String json = server.arg(0);
    StaticJsonDocument<32> doc;
    // Json格式写法,创建一个json消息
    char json_char_[32];
    strcpy(json_char_, json.c_str());
    DeserializationError error = deserializeJson(doc, json_char_);
    Serial.println(doc["info"].as<String>());
    info = doc["info"].as<String>();

    server.sendHeader("Location", "/getinfo");
    server.send(200, "text/plain", "ok");

    if (info == "rotate0")
    {
        tft.setRotation(0);
        tft.fillScreen(TFT_BLACK);
    }
    if (info == "rotate90")
    {
        tft.setRotation(1);
        tft.fillScreen(TFT_BLACK);
    }
    if (info == "rotate180")
    {
        tft.setRotation(2);
        tft.fillScreen(TFT_BLACK);
    }
    if (info == "rotate270")
    {
        tft.setRotation(3);
        tft.fillScreen(TFT_BLACK);
    }
    if (info == "mirror")
    {
        tft.setRotation(4);
        tft.fillScreen(TFT_BLACK);
    }
}
int click_once_count = 0;

void getInfo()
{
    server.send(200, "text/html", (String) "<!DOCTYPE html><body><h1>" + "info: </h1><h1>" + info + "</h1></body></html>");
    Serial.println("------2");
    // getEncodedImage();
    Serial.println(info);
}

void putPageIndex()
{
    Serial.println(server.arg(0));
    String json = server.arg(0);
    StaticJsonDocument<32> doc;
    // Json格式写法,创建一个json消息
    char json_char_[32];
    strcpy(json_char_, json.c_str());
    DeserializationError error = deserializeJson(doc, json_char_);
    Serial.println(doc["info"].as<String>());

    click_once_count = doc["info"].as<String>().toInt();
    Serial.println("------1");
    Serial.println(info);
    server.sendHeader("Location", "/getinfo");
    server.send(302, "text/plain", "ok");
}

//

//

int maxDecodedSize = 1024 * 32;
uint8_t decodedImage[1024 * 32] PROGMEM = {0};
bool show_pic = false;
void handleImageUpload()
{
    if (server.method() == HTTP_POST)
    {
        Serial.println("post1");
        show_pic = true;
        String encodedImage = server.arg("image");
        Serial.println(sizeof(encodedImage));
        int decoded_size = base64_decode((char *)decodedImage, (char *)encodedImage.c_str(), maxDecodedSize);

        Serial.println("post2");
        Serial.println(decoded_size);
        // Save image to LittleFS
        File file = LittleFS.open("/saved_image.bin", "w");
        if (!file)
        {
            Serial.println("Failed to open file for writing");
            return;
        }
        file.write(decodedImage, decoded_size);
        file.close();

        server.send(200, "text/plain", "Image received");
    }
}

#define FORMAT_LITTLEFS_IF_FAILED true

uint8_t frame_0[1024 * 32] PROGMEM = {0};
uint8_t frame_1[1024 * 32] PROGMEM = {0};

void createDir(fs::FS &fs, const char *path)
{
    Serial.printf("Creating Dir: %s\n", path);
    if (fs.mkdir(path))
    {
        Serial.println("Dir created");
    }
    else
    {
        Serial.println("mkdir failed");
    }
}

String *listDir(fs::FS &fs, const char *dirname, uint8_t levels)
{
    Serial.printf("Listing directory: %s\r\n", dirname);
    String dir_list[24];
    File root = fs.open(dirname);
    if (!root)
    {
        Serial.println("- failed to open directory");
        return dir_list;
    }
    if (!root.isDirectory())
    {
        Serial.println(" - not a directory");
        return dir_list;
    }

    File file = root.openNextFile();
    int i = 0;
    while (file)
    {

        if (file.isDirectory())
        {
            Serial.print("  DIR : ");
            Serial.println(file.name());
            dir_list[i] = file.name();
            i++;
            if (levels)
            {
                listDir(fs, file.path(), levels - 1);
            }
        }
        else
        {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            dir_list[i] = file.name();
            i++;
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
        return dir_list;
    }
}

void writeFile(fs::FS &fs, const char *path, const char *message)
{
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if (!file)
    {
        Serial.println("- failed to open file for writing");
        return;
    }
    if (file.print(message))
    {
        Serial.println("- file written");
    }
    else
    {
        Serial.println("- write failed");
    }
    file.close();
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

void clearGifDirectory()
{
    File root = LittleFS.open("/gif");
    if (!root || !root.isDirectory())
    {
        Serial.println("Failed to open directory or not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        Serial.print("Deleting file: ");
        Serial.println(file.name());

        LittleFS.remove(file.name()); // Delete the file
        file = root.openNextFile();   // Move to the next file
    }

    Serial.println("All GIF files cleared.");
}


int frame_num = 0;
int frame_info[] = {0, 240, 0, 0}; // num,w,h,max_num
int image_hight = 0;
int image_max_num = 0;
bool is_upload = false;
void handleGIFUpload()
{
    if (server.method() == HTTP_POST)
    {
        show_pic = false;
        

        String frame_index = server.arg("frame_index");
        String encodedImage = server.arg("image");
        image_hight = server.arg("image_height").toInt();
        image_max_num = server.arg("max_num").toInt();

        Serial.println(frame_index);
        Serial.println("image_hight");
        Serial.println(image_hight);
        Serial.println("image_max_num");
        Serial.println(image_max_num);

        //
        // if(frame_index=="0") {
        //   Serial.println("start remove");
        //   clearGifDirectory();
        // }
// 
        Serial.println(0);
        Serial.println(sizeof(encodedImage));
        if (!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
        {
            Serial.println("LittleFS Mount Failed");
            return;
        }
        createDir(LittleFS, "/gif");
        // Save image height to LittleFS
        writeFile(LittleFS, "/gif/image_height.txt", String(image_hight).c_str());
        writeFile(LittleFS, "/gif/image_max_num.txt", String(image_max_num).c_str());

        


        File file_w = LittleFS.open((char *)("/gif/gif" + frame_index + ".bin").c_str(), "w");
        if (!file_w)
        {
            Serial.println("Failed to open file for writing");
        }
        // 将数据写入文件
        base64::decode((char *)encodedImage.c_str(), frame_0);
        file_w.write(frame_0, sizeof(frame_0));
        file_w.close();
        Serial.println("Data saved to LittleFS.");

        Serial.println("Data saved to LittleFS.");

        frame_num = frame_index.toInt();
        if (frame_num == 0)
            tft.fillScreen(TFT_BLACK);

        Serial.println("frame_num");
        Serial.println(frame_num);
        frame_info[0] = frame_num;
        frame_info[2] = image_hight;
        frame_info[3] = image_max_num;
        Serial.println("frame_info[2]");
        Serial.println(frame_info[2]);

        if (frame_info[0] == frame_info[3] - 1){
          is_upload = false;

        }
        else {is_upload = true;}

        server.send(200, "text/plain", "Image received");
    }
}

void initServer()
{

    server.on("/", HTTP_GET, MainPage);
    server.on("/putinfo", HTTP_POST, putInfo);
    server.on("/putPageIndex", HTTP_POST, putPageIndex);
    server.on("/getinfo", HTTP_GET, getInfo);
    server.on("/upload_image", handleImageUpload);
    server.on("/upload_gif", handleGIFUpload);
    server.begin();
    Serial.println("HTTP server started");
}

//
//
void displaySavedGIF_()
{
    int frameCount = 0;

    // Assuming frames are saved as /gif/gif0.bin, /gif/gif1.bin, etc.
    while (LittleFS.exists("/gif/gif" + String(frameCount) + ".bin")  && frameCount < frame_info[3] - 1 ) {
        File file = LittleFS.open("/gif/gif" + String(frameCount) + ".bin", "r");
        if (!file )
        {
            Serial.println("Failed to open file for reading");
            break;
        }

        size_t fileSize = file.size();
        file.read(frame_1, fileSize);
        int pos_y = 240 - frame_info[2]; // adjust height if needed

        TJpgDec.drawJpg(0, (int)(abs(pos_y) / 2), frame_1, sizeof(frame_1));
        file.close();

        frameCount++;
        delay(10); // adjust delay for smooth animation
    }
}





void displaySavedGIF() {
    int frameCount = 0;
    mpu6050.update();
    float angleZ = mpu6050.getAngleZ();

    // Map angleZ from -90 to 90 to frame index from 0 to 15
    int frameIndex = map(abs((int)angleZ%180), 0, 180, 0, 35);
    Serial.print("Current Frame Index: ");
    Serial.println(frameIndex);

    // Assuming frames are saved as /gif/gif0.bin, /gif/gif1.bin, etc.
    if (frameIndex < frame_info[3] && frameIndex >= 0) {
        File file = LittleFS.open("/gif/gif" + String(frameIndex) + ".bin", "r");
        if (!file) {
            Serial.println("Failed to open file for reading");
            return;
        }

        size_t fileSize = file.size();
        file.read(frame_1, fileSize);
        file.close();

        int pos_y = 20+240 - frame_info[2]; // Adjust height if needed
        TJpgDec.drawJpg(0, (int)(abs(pos_y) / 2), frame_1, sizeof(frame_1));
    }
}





//
//
void displaySavedImage()
{
    File file = LittleFS.open("/saved_image.bin", "r");
    if (!file)
    {
        Serial.println("Failed to open saved image file for reading");
        return;
    }

    size_t fileSize = file.size();
    file.read(decodedImage, fileSize);
    file.close();

    uint16_t w = 0, h = 0;
    TJpgDec.getJpgSize(&w, &h, decodedImage, sizeof(decodedImage));
    TJpgDec.drawJpg(0, 0, decodedImage, sizeof(decodedImage));
}

void setup(void)
{
    Serial.begin(115200);
    Serial.println(0);
    tft.init();
    tft.setRotation(0);
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(1);//90

    // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
    TJpgDec.setJpgScale(1);

    // The byte order can be swapped (set true for TFT_eSPI)
    TJpgDec.setSwapBytes(true);

    // The decoder must be given the exact name of the rendering function above
    TJpgDec.setCallback(tft_output);


    screenInfo("Hello", 50, 110, 4);

    Wire.begin(SDA_PIN, SCL_PIN);
    mpu6050.begin();
    mpu6050.calcGyroOffsets(true);

    Serial.println(2);
    initWIFI();
    Serial.println(3);
    initServer();
    Serial.println(4);

// Initialize LittleFS
    if (LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED))
    {
        String saved_image_height = readFile(LittleFS, "/gif/image_height.txt");
        String saved_image_max_num = readFile(LittleFS, "/gif/image_max_num.txt");
        if (saved_image_height.length() > 0)
        {
            frame_info[2] = saved_image_height.toInt(); // Load the saved image height
            Serial.print("Loaded image height: ");
            Serial.println(frame_info[2]);
            frame_info[3] = saved_image_max_num.toInt(); // Load the saved image height
        }
        else
        {
            Serial.println("No saved image height found.");
        }
    }
    else
    {
        Serial.println("LittleFS Mount Failed");
    }

    // Check if there is any saved GIF frames or image
    if (LittleFS.exists("/gif/gif0.bin")) {
        // Display the GIF if it exists
        displaySavedGIF_();
    } else if (LittleFS.exists("/saved_image.bin")) {
        // Display the saved image if it exists
        displaySavedImage();
    }

    // Serial.println(readFile(LittleFS, "/gifinfo.txt"));
    Serial.println("###############################");

}

void loop()
{
    // put your main code here, to run repeatedly:

    server.handleClient();
    

    if (show_pic == false )
    {
        if (frame_info[0] < frame_info[3] - 1 && is_upload == true)
            screenInfo(String(frame_info[0]+1) +"/"+String(frame_info[3])+" Uploading...", 50, 110, 4);

        else
        {

            displaySavedGIF();
        }
    }

    else {
      
        if (decodedImage[0]!=0)
            { 
              show_pic == false;

              displaySavedImage();
              
            }
            else delay(100);     
      
      
      }

    delay(4);
}
