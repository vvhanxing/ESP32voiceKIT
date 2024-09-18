#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient_Generic.h>
#include <SocketIOclient_Generic.h>
#include <driver/i2s.h>
#include <ArduinoJson.h>

WiFiMulti WiFiMulti;
SocketIOclient socketIO;

IPAddress serverIP(192, 168, 43, 220);  // 服务器IP
uint16_t serverPort = 5002;             // WebSocket 服务器端口

#define I2S_WS 8
#define I2S_SD 13
#define I2S_SCK 7
#define I2S_PORT I2S_NUM_0
#define bufferLen 512  // I2S 缓冲区

int16_t sBuffer[bufferLen];
uint32_t sequenceNumber = 0;  // 序列号

char ssid[] = "HUAWEI P50 Pro";  // WiFi 名称
char pass[] = "12345678";        // WiFi 密码

bool isRecording = false;         // 当前是否正在录音
unsigned long silenceDuration = 0;  // 记录静音持续时间
unsigned long lastSoundTime = 0;
const unsigned long maxSilence = 2000;  // 最大静音时间 2 秒
const float threshold = 500;  // 声音阈值

void socketIOEvent(const socketIOmessageType_t& type, uint8_t *payload, const size_t& length) {
  switch (type) {
    case sIOtype_DISCONNECT:
      Serial.println("[IOc] Disconnected");
      break;
    case sIOtype_CONNECT:
      Serial.print("[IOc] Connected to url: ");
      Serial.println((char*) payload);
      socketIO.send(sIOtype_CONNECT, "/");
      break;
    case sIOtype_EVENT:
      Serial.print("[IOc] Get event: ");
      Serial.println((char*) payload);
      break;
    default:
      break;
  }
}

void i2s_install() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,  // 采样率16kHz
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };
  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
  i2s_set_pin(I2S_PORT, &pin_config);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  WiFiMulti.addAP(ssid, pass);
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected");

  socketIO.begin(serverIP, serverPort);
  socketIO.onEvent(socketIOEvent);

  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);
}

void startRecording() {
  isRecording = true;
  Serial.println("Start recording...");
  sequenceNumber = 0;
}

void stopRecording() {
  isRecording = false;
  Serial.println("Stop recording...");

  // 修改后的JSON格式
        DynamicJsonDocument doc(1024);
        JsonArray array = doc.to<JsonArray>();

        array.add("stop_recording");
        String output;
        serializeJson(doc, output);
  
  // 调试输出，检查 JSON 格式
  Serial.print("Sending stop recording event: ");
  Serial.println(output);
  
  socketIO.sendEVENT(output);  // 发送事件到后端
}

void loop() {
  socketIO.loop();

  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);

  if (result == ESP_OK) {
    int samples_read = bytesIn / 2;
    if (samples_read > 0) {
      float mean = 0;
      for (int i = 0; i < samples_read; ++i) {
        mean += abs(sBuffer[i]);
      }
      mean /= samples_read;

      // 如果声音超过阈值，开始录音
      if (mean > threshold) {
        lastSoundTime = millis();
        if (!isRecording) {
          startRecording();
        }
      }

      // 如果正在录音，且超过2秒没有声音，则停止录音
      if (isRecording && millis() - lastSoundTime > maxSilence) {
        stopRecording();
      }

      // 录音过程中发送音频数据
      if (isRecording) {
        DynamicJsonDocument doc(1024);
        JsonArray array = doc.to<JsonArray>();

        array.add("audio_data");

        JsonObject dataObject = array.createNestedObject();
        dataObject["sequence_number"] = sequenceNumber++;  // 添加序列号

        JsonArray dataArray = dataObject.createNestedArray("samples");
        for (int i = 0; i < samples_read; ++i) {
          dataArray.add(sBuffer[i]);
        }

        String output;
        serializeJson(doc, output);
        socketIO.sendEVENT(output);

        Serial.print("Audio data sent with sequence number: ");
        Serial.println(sequenceNumber - 1);
      }
    }
  }
}
