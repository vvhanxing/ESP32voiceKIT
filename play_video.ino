#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
///
#define COLOR_BACKGROUND TFT_WHITE
#define COLOR_TEXT       TFT_BLACK
#define COLOR_HEADER     TFT_BLUE
#define COLOR_BORDER     TFT_DARKGREY
File myFile; 
String fileList [512] = {""};
int fileIndex = 0;
String dirList [512] = {""};
int dirIndex = 0;
// 定义一个自定义的SPI类实例
SPIClass CustomSPI;
/**
 * @brief 将指定消息写入到文件中。
 * 
 * @param path 指向要写入的文件路径的字符指针。
 * @param message 指向要写入文件的消息的字符指针。
 * 说明：函数不返回任何值，但会在串口打印操作的结果。
 */
void WriteFile(const char *path, const char *message)
{
  // 尝试打开文件以便写入
  myFile = SD.open(path, FILE_WRITE);
  if (myFile)
  {
    // 打印正在写入的文件路径
    Serial.printf("Writing to %s ", path);
    // 写入消息到文件，并在末尾添加换行符
    myFile.println(message);
    // 关闭文件
    myFile.close(); 
    // 打印写入操作完成的通知
    Serial.println("completed.");
  }

  else
  {
    // 打印打开文件失败的通知
    Serial.println("error opening file ");
    // 打印失败的文件路径
    Serial.println(path);
  }
}


const int CS = 7; // 定义SPI通信中CS（Chip Select）信号的常量
const int MOSI_PIN = 8; // 定义SPI通信中MOSI（Master Out Slave In）信号的常量
const int SCK_PIN = 9; // 定义SPI通信中SCK（Serial Clock）信号的常量
const int MISO_PIN = 10; // 定义SPI通信中MISO（Master In Slave Out）信号的常量

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);
    
    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return ;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return ;
    }
    fileIndex = 0;
    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            dirList[dirIndex] = file.name();
            dirIndex+=1;
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
            fileList[fileIndex] = file.name();
            fileIndex+=1;
        }
        file = root.openNextFile();
    }
}

void createDir(fs::FS &fs, const char * path){
    Serial.printf("Creating Dir: %s\n", path);
    if(fs.mkdir(path)){
        Serial.println("Dir created");
    } else {
        Serial.println("mkdir failed");
    }
}


String ReadFile_txt(const char *path)
{
  myFile = SD.open(path); // 尝试打开文件
  String result = "";
  if (myFile) // 如果文件成功打开
  {
    Serial.printf("Reading file from %s\n", path); // 打印读取文件的路径信息
    while (myFile.available()) // 循环读取文件中的所有内容
    {
      //Serial.write(myFile.read()); // 将读取到的每个字节通过串口发送
      result  += (char)myFile.read();
    }
    myFile.close(); // 关闭文件
    return result;
  }
  else // 如果文件打开失败
  {
    Serial.println("error opening :"); // 打印错误信息
    Serial.println(path); // 打印错误信息
    Serial.println("====="); // 打印错误信息
    return result;
  }
}


#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in User_Setup.h

// JPEG decoder library
// Include the jpeg decoder library
#include <TJpg_Decoder.h>
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
   // Stop further decoding as image is running off bottom of screen
  if ( y >= tft.height() ) return 0;

  // This function will clip the image block rendering automatically at the TFT boundaries
  tft.pushImage(x, y, w, h, bitmap);

  // This might work instead if you adapt the sketch to use the Adafruit_GFX library
  // tft.drawRGBBitmap(x, y, bitmap, w, h);

  // Return 1 to decode next block
  return 1;
}
#include "arduino_base64.hpp"
uint8_t frame_0[1024*32 ] PROGMEM ={0};
size_t fileSize = sizeof(frame_0);

void draw_pic_base64(char* pic_name){
  String picTxt =ReadFile_txt(  pic_name);
  base64::decode((char*) picTxt.c_str(), frame_0);
  TJpgDec.drawJpg(  0, 120-50,frame_0, sizeof(frame_0));
  Serial.println("Reading from test.txt");


}
void draw_pic_bin(const char *pic_name) {
    myFile = SD.open(pic_name, FILE_READ); // 尝试打开二进制文件
    if (myFile) {

        myFile.read(frame_0, fileSize);
        // 在TFT上绘制JPEG图片
        TJpgDec.drawJpg(0, 0, frame_0, fileSize);
        myFile.close(); // 关闭文件

    } else {
        Serial.println("Error opening binary file:");
        Serial.println(pic_name);
    }
}


void screenInfo(String info, int posx, int posy, int font)
{

    tft.setCursor(posx, posy);
    // 设置文本颜色为白色,黑色文本背景
    tft.setTextFont(font);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    // 设置显示的文字,注意这里有个换行符 \n 产生的效果
    tft.println(info);
}


void setup() {
  // put your setup code here, to run once:
  // 初始化串口通信，设置波特率为9600，并延迟500ms
  Serial.begin(9600);
  delay(500);

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_WHITE);

  // The jpeg image can be scaled by a factor of 1, 2, 4, or 8
  TJpgDec.setJpgScale(1);

  // The byte order can be swapped (set true for TFT_eSPI)
  TJpgDec.setSwapBytes(true);

  // The decoder must be given the exact name of the rendering function above
  TJpgDec.setCallback(tft_output);

  tft.setTextColor(TFT_BLACK, TFT_WHITE);

  // 初始化自定义SPI通信，传入SCK、MISO、MOSI引脚及CS控制信号
  CustomSPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS);
  while (!Serial)
  {
    ;
  } 

  // 打印初始化SD卡的开始信息
  Serial.println("Initializing SD card...");
  
  // 初始化SD卡，如果初始化失败则打印错误信息并返回
  if (!SD.begin(CS, CustomSPI, 4000000))
  {
    Serial.println("initialization failed!");
    return;
  }
  
  // 打印SD卡初始化成功的消息
  Serial.println("initialization done.");

  // 写入测试文件"/test.txt"
  //WriteFile("/sys/mydata/txt4.txt", "hello worlD");
  // 读取测试文件"/test.txt"的内容

  //打印读取到的内容
  Serial.println("Reading from test.txt");


  tft.fillScreen(TFT_BLACK);
  
  


}

void loop() {
    listDir(SD,"/move1",0);  
    String sysPath = "/move1/";
    int frame = 1;
    
    while (true) {
        // Construct the filename for the current frame
        String filename = sysPath + String(frame) + ".bin";
        
        // Draw the binary frame from SD card
        draw_pic_bin(filename.c_str());

        // Increment frame index, loop back to 1 if it exceeds 30
        frame = (frame % fileIndex) + 1;
    }

}
