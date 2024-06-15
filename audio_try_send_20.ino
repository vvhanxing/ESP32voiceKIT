#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <Arduino.h>
#include <driver/i2s.h>

#include <WebServer.h>
#include <DNSServer.h>

WebServer server(80);  // 创建Web服务器对象
DNSServer dnsServer;   // 创建DNS服务器对象
int isConnectedWIFI =0;
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <title>ESP32 WiFi Configuration</title>
  </head>
  <body>
    <h1>ESP32 WiFi Configuration</h1>
    <form action="/config" method="post">
      <label for="ssid">SSID:</label><br>
      <input type="text" id="ssid" name="ssid"><br>
      <label for="password">Password:</label><br>
      <input type="password" id="password" name="password"><br><br>
      <input type="submit" value="Submit">
    </form>
  </body>
</html>
)rawliteral";

// WiFi网络设置
const char* ssid = "AI BOT";
const char* password = "12345678";
int LED = 2;

String HOMEssid = "";
String HOMEpassword = "";

void connectToWiFi(const char* ssid, const char* password) {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  isConnectedWIFI=1;
}


//播放MP3设置
#include <Arduino.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
//#include "AudioOutputI2SNoDAC.h"
#include "AudioOutputI2S.h"
//// Flask服务器播放mp3 ip地址
const char *URL="http://47.117.155.179/audio/mp3";

AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
//AudioOutputI2SNoDAC *out;
AudioOutputI2S *out;

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string)
{
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void) isUnicode; // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2)-1]=0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  if (code != 257 && code != 565 && code != 260) {

  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1)-1]=0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
  }
}


#define I2S_PORT I2S_NUM_0

// 播放设置
void initURLaudio(){

 // 初始化I2S
   i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX |I2S_MODE_RX),
    .sample_rate =  44100,              // The format of the signal using ADC_BUILT_IN
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 32,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
   };
   i2s_driver_uninstall(I2S_NUM_0);
   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  
 // i2s_set_pin((i2s_port_t)I2S_PORT2, NULL);//i2s_driver_uninstall((i2s_port_t)I2S_PORT2);
//
//  // 设置I2S的引脚连接
//  i2s_set_pin(I2S_NUM_0, NULL);

  audioLogger = &Serial;
  file = new AudioFileSourceICYStream(URL);
  file->RegisterMetadataCB(MDCallback, (void*)"ICY");
  buff = new AudioFileSourceBuffer(file, 1024);
  buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
//  out = new AudioOutputI2SNoDAC();
  out = new AudioOutputI2S();
  out -> SetGain(1.0);            //设置音量
  out -> SetPinout(12,26,13);     //设置接到MAX98357A的引脚, GPIO12(串行时钟SCK)-->SCLK, GPIO26(字选择WS)-->LRC, GPIO13(串行数据SD)-->DIN
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  mp3->begin(buff, out);
  }

bool play_mp3_ready = false;
bool strat_init_audio = true;

void loopURLaudio() {
  static int lastms = 0;

  if (mp3->isRunning()) {
    if (millis()-lastms > 1000) {
      lastms = millis();
      //Serial.printf("Running for %d ms...\n", lastms);
      Serial.flush();
     }
    if (!mp3->loop()) mp3->stop();
  } else {
    Serial.printf("MP3 done\n");
    delay(1000);
    play_mp3_ready=false;
  }
}





// Flask服务器录音ip地址
const char* serverAddress = "http://47.117.155.179/record";

// INMP441麦克风设置
#define SAMPLE_RATE     (16000)
#define SAMPLE_SIZE     (1024*2)  // 减小采样大小以减少延迟

// 音频数据缓冲区
uint8_t sample_buffer[SAMPLE_SIZE];
//按照接线确定编号
//#define I2S_WS 15
//#define I2S_SD 32
//#define I2S_SCK 14

#define I2S_WS 21
#define I2S_SD 32
#define I2S_SCK 19
// 使用I2S处理器
//#define I2S_PORT I2S_NUM_0

void initI2S() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER  |I2S_MODE_RX | I2S_MODE_TX), // 接收模式
    .sample_rate = SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT, // 只采集左声道
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 1024,
    .use_apll = false
  };

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &pin_config);
}

// 定义缓冲区长度
#define bufferLen_ 64
int16_t sBuffer_[bufferLen_];

// 定义阈值和计时变量
const float threshold = 100; // 设置阈值
unsigned long lastAboveThresholdTime = 0; // 上次超过阈值的时间
bool printEnabled = true; // 控制录音的开关

void collectAndSendAudio() {
  
    size_t bytesIn = 0;
    esp_err_t result_0 = i2s_read(I2S_PORT, &sBuffer_, bufferLen_, &bytesIn, portMAX_DELAY);
    if (result_0 == ESP_OK)
    {
      // 读取I2S数据缓冲区
      int16_t samples_read = bytesIn / 8;
      if (samples_read > 0) {
        float mean = 0;
        for (int16_t i = 0; i < samples_read; ++i) {
          mean += (sBuffer_[i]);
        }
        // 取数据读数的平均值
        mean /= samples_read;

        // 如果音频信号大于阈值，打印数值
        if (mean > threshold) {
          Serial.println("============samples_read");
          Serial.println(samples_read);
          Serial.println("============mean");
          Serial.println(mean);
          lastAboveThresholdTime = millis(); // 更新最后超过阈值的时间
          printEnabled = true; // 使能打印
//          digitalWrite(LED, HIGH);
        } else {
          // 如果连续三秒没有超过阈值，则停止打印
          if (millis() - lastAboveThresholdTime > 3000 && printEnabled) {
            Serial.println("Stopped printing due to no signal above threshold for 3 seconds");
            printEnabled = false; // 关闭打印  #结束完成后post信息，后端判断声音时间长度来决定立即返回还是翻译文本返回
            GetText();
//            digitalWrite(LED, LOW);
            
          }
        }
      }
    }



    
    esp_err_t result = i2s_read(I2S_PORT, sample_buffer, SAMPLE_SIZE, &bytesIn, portMAX_DELAY);
    int stop_recoeding = -1;
    if (printEnabled){//发送录音
      stop_recoeding= 0;
         if (result == ESP_OK ) {
            // 创建HTTP客户端
            HTTPClient http;
            

            // 设置HTTP请求头
            http.begin(serverAddress);
            http.addHeader("Content-Type", "application/octet-stream");
            
            // 发送音频数据
            int httpResponseCode = http.POST(sample_buffer, SAMPLE_SIZE);

            // 检查HTTP响应
            if (httpResponseCode > 0) {
              Serial.print("HTTP Response code: ");
              Serial.println(httpResponseCode);
              //String payload = http.getString();
              //Serial.println(payload); // 打印服务器返回的内容
            } else {
              Serial.print("Error code: ");
              Serial.println(httpResponseCode);
            }

            // 关闭HTTP连接
            http.end();
            
      
      
    }
    }
//    else {
//      if (stop_recoeding==0)
//      {
//        Serial.print("stop_recoeding ");
//         
//        stop_recoeding=1;}
//      
//      }

}
//#####################################################################
void GetText(){
  Serial.print("Connecting to website: ");
    HTTPClient http;
    http.setTimeout(20000);
    http.begin("http://47.117.155.179/audio2text"); //HTTPS example connection
    //http.begin("http://www.arduino.php5.sk/rele/rele1.txt"); //HTTP example connection
    //if uncomment HTTP example, you can comment root CA certificate too!
    int httpCode = http.GET();
    if(httpCode > 0) {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      //file found at server --> on unsucessful connection code will be -1
      if(httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.println(payload);
        if (payload=="start speaking") {
                play_mp3_ready=true;
                initURLaudio();
                }
      }
     }else{
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
  http.end();  
  delay(200);
}
//#######################
void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  // 初始化WiFi连接
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlPage);
  });

  server.on("/config", HTTP_POST, []() {
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    String response = "SSID: " + ssid + "<br>Password: " + password;
    server.send(200, "text/html", response);
    delay(1000);
    HOMEssid = ssid;
    HOMEpassword = password;
    connectToWiFi(ssid.c_str(), password.c_str());
  });

  server.begin();
  Serial.println("HTTP server started");
  
}
int click_push_count = 0;
int click_once_count = 0;
int array [] ={0,0,0,0,0,0};
bool click();
void collectAndSendAudio();
void loop() {

  if (isConnectedWIFI==0){
      dnsServer.processNextRequest();
      server.handleClient();
      }

  else if(WiFi.status() != WL_CONNECTED){
      //WiFi.softAPdisconnect(true);
      WiFi.mode(WIFI_STA);
      Serial.println(HOMEssid);
      Serial.println(HOMEpassword);
      WiFi.begin(HOMEssid.c_str(), HOMEpassword.c_str());
      Serial.print("Connecting to WiFi...");
      while (WiFi.status() != WL_CONNECTED) {
          delay(500);
          Serial.print("*");
      }
      Serial.println(" connected");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
      isConnectedWIFI=1;    
    
    }

  else {
      if (play_mp3_ready==false){
          if (strat_init_audio==true){
              initI2S();
              strat_init_audio=false;
          }
      collectAndSendAudio();}

      else {loopURLaudio();
          strat_init_audio = true;}
      }

 

}


bool click(){

   const int touchPin = 4; // 使用 T0 获取数据
   const int threshold = 40;
   // variable for storing the touch pin value 
   int touchValue;
  // read the state of the pushbutton value:
  touchValue = touchRead(touchPin);
  //Serial.print(touchValue);
  //Serial.print("\n");

  // check if the touchValue is below the threshold
  // if it is, set ledPin to HIGH
  if(touchValue < threshold){
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
