import wave
import numpy as np
from flask import Flask
from flask_socketio import SocketIO, emit
import wave
import sys
import json

from vosk import Model, KaldiRecognizer, SetLogLevel
SetLogLevel(-1)
model = Model(model_name="vosk-model-small-cn-0.22")

app = Flask(__name__)
app.secret_key = 'your_secret_key'
socketio = SocketIO(app, cors_allowed_origins="*")

SAMPLE_RATE = 16000
CHANNELS = 1
SAMPLE_WIDTH = 2
audio_data = []
last_sequence_number = -1

@socketio.on('connect')
def handle_connect():
    print('Client connected')
    emit('response', {'message': 'Connected to WebSocket server'})

@socketio.on('audio_data')
def handle_audio_data(data):
    global last_sequence_number
    sequence_number = data["sequence_number"]
    audio_samples = np.array(data["samples"], dtype=np.int16)

    print(f'Received sequence number: {sequence_number}')
    if last_sequence_number != -1 and sequence_number != last_sequence_number + 1:
        missing_packets = sequence_number - last_sequence_number - 1
        print(f'Warning: {missing_packets} packets were lost!')
        audio_data.extend([0] * len(audio_samples) * missing_packets)

    audio_data.extend(audio_samples)
    last_sequence_number = sequence_number

@socketio.on('stop_recording')
def handle_stop_recording():
    # 调试输出接收到的数据，检查 JSON 格式
    print(f'Stop recording')

    save_wav_file('output.wav', audio_data)
    str_ret= stt('output.wav')
    print(str_ret)
    emit('response', {'message': 'Audio file saved successfully'})
    audio_data.clear()


def save_wav_file(filename, audio_data):
    with wave.open(filename, 'w') as wf:
        wf.setnchannels(CHANNELS)
        wf.setsampwidth(SAMPLE_WIDTH)
        wf.setframerate(SAMPLE_RATE)
        wf.writeframes(np.array(audio_data, dtype=np.int16).tobytes())
    print(f'Audio saved to {filename}')
def stt(filename):
    wf = wave.open(filename, "rb")
    if wf.getnchannels() != 1 or wf.getsampwidth() != 2 or wf.getcomptype() != "NONE":
        print("Audio file must be WAV format mono PCM.")

        sys.exit(1)
    rec = KaldiRecognizer(model, wf.getframerate())
    rec.SetWords(True)
    # rec.SetPartialWords(True)   # 注释这行   《《《《

    str_ret = ""

    while True:
        data = wf.readframes(4000)
        if len(data) == 0:
            break
        if rec.AcceptWaveform(data):
            result = rec.Result()
            # print(result)

            result = json.loads(result)
            if 'text' in result:
                str_ret += result['text'] + ' '
        # else:
        #     print(rec.PartialResult())

    # print(rec.FinalResult())
    result = json.loads(rec.FinalResult())
    if 'text' in result:
        str_ret += result['text']

    return str_ret

@app.route('/')
def index():
    return "WebSocket server is running"

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5002, debug=True)



# #include <WiFi.h>
# #include <WiFiMulti.h>
# #include <WiFiClientSecure.h>
# #include <WebSocketsClient_Generic.h>
# #include <SocketIOclient_Generic.h>
# #include <driver/i2s.h>
# #include <ArduinoJson.h>

# WiFiMulti WiFiMulti;
# SocketIOclient socketIO;

# IPAddress serverIP(192, 168, 43, 220);  // 服务器IP
# uint16_t serverPort = 5002;             // WebSocket 服务器端口

# #define I2S_WS 8
# #define I2S_SD 13
# #define I2S_SCK 7
# #define I2S_PORT I2S_NUM_0
# #define bufferLen 512  // I2S 缓冲区

# int16_t sBuffer[bufferLen];
# uint32_t sequenceNumber = 0;  // 序列号

# char ssid[] = "HUAWEI P50 Pro";  // WiFi 名称
# char pass[] = "12345678";        // WiFi 密码

# bool isRecording = false;         // 当前是否正在录音
# unsigned long silenceDuration = 0;  // 记录静音持续时间
# unsigned long lastSoundTime = 0;
# const unsigned long maxSilence = 2000;  // 最大静音时间 2 秒
# const float threshold = 500;  // 声音阈值

# void socketIOEvent(const socketIOmessageType_t& type, uint8_t *payload, const size_t& length) {
#   switch (type) {
#     case sIOtype_DISCONNECT:
#       Serial.println("[IOc] Disconnected");
#       break;
#     case sIOtype_CONNECT:
#       Serial.print("[IOc] Connected to url: ");
#       Serial.println((char*) payload);
#       socketIO.send(sIOtype_CONNECT, "/");
#       break;
#     case sIOtype_EVENT:
#       Serial.print("[IOc] Get event: ");
#       Serial.println((char*) payload);
#       break;
#     default:
#       break;
#   }
# }

# void i2s_install() {
#   const i2s_config_t i2s_config = {
#     .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
#     .sample_rate = 16000,  // 采样率16kHz
#     .bits_per_sample = i2s_bits_per_sample_t(16),
#     .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
#     .communication_format = I2S_COMM_FORMAT_STAND_I2S,
#     .intr_alloc_flags = 0,
#     .dma_buf_count = 8,
#     .dma_buf_len = bufferLen,
#     .use_apll = false
#   };
#   i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
# }

# void i2s_setpin() {
#   const i2s_pin_config_t pin_config = {
#     .bck_io_num = I2S_SCK,
#     .ws_io_num = I2S_WS,
#     .data_out_num = -1,
#     .data_in_num = I2S_SD
#   };
#   i2s_set_pin(I2S_PORT, &pin_config);
# }

# void setup() {
#   Serial.begin(115200);
#   delay(200);

#   WiFiMulti.addAP(ssid, pass);
#   while (WiFiMulti.run() != WL_CONNECTED) {
#     Serial.print(".");
#     delay(500);
#   }
#   Serial.println("\nWiFi connected");

#   socketIO.begin(serverIP, serverPort);
#   socketIO.onEvent(socketIOEvent);

#   i2s_install();
#   i2s_setpin();
#   i2s_start(I2S_PORT);
# }

# void startRecording() {
#   isRecording = true;
#   Serial.println("Start recording...");
#   sequenceNumber = 0;
# }

# void stopRecording() {
#   isRecording = false;
#   Serial.println("Stop recording...");

#   // 修改后的JSON格式
#         DynamicJsonDocument doc(1024);
#         JsonArray array = doc.to<JsonArray>();

#         array.add("stop_recording");
#         String output;
#         serializeJson(doc, output);
  
#   // 调试输出，检查 JSON 格式
#   Serial.print("Sending stop recording event: ");
#   Serial.println(output);
  
#   socketIO.sendEVENT(output);  // 发送事件到后端
# }

# void loop() {
#   socketIO.loop();

#   size_t bytesIn = 0;
#   esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);

#   if (result == ESP_OK) {
#     int samples_read = bytesIn / 2;
#     if (samples_read > 0) {
#       float mean = 0;
#       for (int i = 0; i < samples_read; ++i) {
#         mean += abs(sBuffer[i]);
#       }
#       mean /= samples_read;

#       // 如果声音超过阈值，开始录音
#       if (mean > threshold) {
#         lastSoundTime = millis();
#         if (!isRecording) {
#           startRecording();
#         }
#       }

#       // 如果正在录音，且超过2秒没有声音，则停止录音
#       if (isRecording && millis() - lastSoundTime > maxSilence) {
#         stopRecording();
#       }

#       // 录音过程中发送音频数据
#       if (isRecording) {
#         DynamicJsonDocument doc(1024);
#         JsonArray array = doc.to<JsonArray>();

#         array.add("audio_data");

#         JsonObject dataObject = array.createNestedObject();
#         dataObject["sequence_number"] = sequenceNumber++;  // 添加序列号

#         JsonArray dataArray = dataObject.createNestedArray("samples");
#         for (int i = 0; i < samples_read; ++i) {
#           dataArray.add(sBuffer[i]);
#         }

#         String output;
#         serializeJson(doc, output);
#         socketIO.sendEVENT(output);

#         Serial.print("Audio data sent with sequence number: ");
#         Serial.println(sequenceNumber - 1);
#       }
#     }
#   }
# }
