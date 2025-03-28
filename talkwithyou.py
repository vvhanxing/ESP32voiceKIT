import wave
import numpy as np
from flask import Flask, request, send_file
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

@app.route('/tts')
def stream_mp3():

    audio_file = './tmp2.mp3'
    # ffmpeg_MP3ToWav(input_path, output_path)
    return send_file(audio_file, mimetype='audio/mpeg')


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
    # str_ret= stt('output.wav')
    str_ret = stt('output.wav')
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
# from faster_whisper import WhisperModel
# import wave
# model_size = "./whisper-small-zh-CN-ct2"
# # model_size = "whisper-tiny-zh-ct2-int8"

# # Run on GPU with FP16
# #model = WhisperModel(model_size, device="cuda", compute_type="float16")
# model = WhisperModel(model_size, device="cpu", compute_type="int8")

# def stt2(filename):
#     segments, info = model.transcribe(filename, beam_size=5, language='zh',initial_prompt = "以下是普通话的句子。")
#     print("Detected language '%s' with probability %f" % (info.language, info.language_probability))
#     stt_text = ""
#     for segment in segments:
#         print("[%.2fs -> %.2fs] %s" % (segment.start, segment.end, segment.text ))  
#         stt_text+= segment.text
#     if "普通话是普通话" in stt_text:
#         stt_text = "主人来啦"
#     print(stt_text)

@app.route('/')
def index():
    return "WebSocket server is running"

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5002, debug=True)
#####################################

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiClientSecure.h>
#include <WebSocketsClient_Generic.h>
#include <SocketIOclient_Generic.h>
#include <driver/i2s.h>
#include <ArduinoJson.h>

WiFiMulti WiFiMulti;
SocketIOclient socketIO;

IPAddress serverIP(192, 168, 43, 220);  // 服务器IP    serverIP(118,31,40,215);        serverIP(192, 168, 43, 220);
uint16_t serverPort = 5002;              // WebSocket 服务器端口

#define I2S_WS 8
#define I2S_SD 13
#define I2S_SCK 7
#define I2S_PORT I2S_NUM_0
#define bufferLen 512  // I2S 缓冲区

int16_t sBuffer[bufferLen];
uint32_t sequenceNumber = 0;  // 序列号

char ssid[] = "HUAWEI P50 Pro";   // WiFi 名称      "HUAWEI P50 Pro"
char pass[] = "12345678";  // WiFi 密码       "12345678"

// const char *ssid = "HUAWEI P50 Pro"; // Enter your SSID here
// const char *password = "12345678";   // Enter your Password here


bool isRecording = false;           // 当前是否正在录音
unsigned long silenceDuration = 0;  // 记录静音持续时间
unsigned long lastSoundTime = 0;
const unsigned long maxSilence = 1500;  // 最大静音时间 2 秒
const float threshold = 200;           // 声音阈值




bool play_mp3_ready = false;
bool strat_init_audio = true;
bool have_positive = false;




void socketIOEvent(const socketIOmessageType_t &type, uint8_t *payload, const size_t &length) {
  switch (type) {
    case sIOtype_DISCONNECT:
      Serial.println("[IOc] Disconnected");
      break;
    case sIOtype_CONNECT:
      Serial.print("[IOc] Connected to url: ");
      Serial.println((char *)payload);
      socketIO.send(sIOtype_CONNECT, "/");
      break;
    case sIOtype_EVENT:
      Serial.print("[IOc] Get event: ");
      Serial.println((char *)payload);
      play_mp3_ready = true;




      break;
    default:
      break;
  }
}

//播放MP3设置
#include <Arduino.h>
#include "AudioFileSourceICYStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
//#include "AudioOutputI2SNoDAC.h"
#include "AudioOutputI2S.h"
//// Flask服务器播放mp3 ip地址


AudioGeneratorMP3 *mp3;
AudioFileSourceICYStream *file;
AudioFileSourceBuffer *buff;
//AudioOutputI2SNoDAC *out;
AudioOutputI2S *out;

// Called when a metadata event occurs (i.e. an ID3 tag, an ICY block, etc.
void MDCallback(void *cbData, const char *type, bool isUnicode, const char *string) {
  const char *ptr = reinterpret_cast<const char *>(cbData);
  (void)isUnicode;  // Punt this ball for now
  // Note that the type and string may be in PROGMEM, so copy them to RAM for printf
  char s1[32], s2[64];
  strncpy_P(s1, type, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  strncpy_P(s2, string, sizeof(s2));
  s2[sizeof(s2) - 1] = 0;
  Serial.printf("METADATA(%s) '%s' = '%s'\n", ptr, s1, s2);
  Serial.flush();
}

// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string) {
  if (code != 257 && code != 565 && code != 260) {

    const char *ptr = reinterpret_cast<const char *>(cbData);
    // Note that the string may be in PROGMEM, so copy it to RAM for printf
    char s1[64];
    strncpy_P(s1, string, sizeof(s1));
    s1[sizeof(s1) - 1] = 0;
    Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
    Serial.flush();
  }
}

#define I2S_PORT I2S_NUM_0

// 播放设置
void initURLaudio() {

  // 初始化I2S
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
    .sample_rate = 44100,                          // The format of the signal using ADC_BUILT_IN
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,  // is fixed at 12bit, stereo, MSB
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

  const char *URL = "http://192.168.43.220:5002/tts";
  file = new AudioFileSourceICYStream(URL);
  file->RegisterMetadataCB(MDCallback, (void *)"ICY");
  buff = new AudioFileSourceBuffer(file, 1024);
  buff->RegisterStatusCB(StatusCallback, (void *)"buffer");
  out = new AudioOutputI2S();
  out->SetGain(1.0);           //设置音量
  out->SetPinout(11, 12, 10);  //设置接到MAX98357A的引脚, GPIO12(串行时钟SCK)-->SCLK, GPIO26(字选择WS)-->LRC, GPIO13(串行数据SD)-->DIN
  mp3 = new AudioGeneratorMP3();
  mp3->RegisterStatusCB(StatusCallback, (void *)"mp3");
  mp3->begin(buff, out);
}


void loopURLaudio() {
  static int lastms = 0;

  if (mp3->isRunning()) {
    if (millis() - lastms > 1000) {
      lastms = millis();
      //Serial.printf("Running for %d ms...\n", lastms);
      Serial.flush();
    }
    if (!mp3->loop()) mp3->stop();
  } else {
    Serial.printf("MP3 done\n");
    delay(1000);
    play_mp3_ready = false;
    isRecording = false;
    have_positive = false;
  }
}

void i2s_RX_install() {
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,  // 采样率设置为16kHz
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags =  ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };
  i2s_driver_uninstall(I2S_NUM_0);
  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);

  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };
  i2s_set_pin(I2S_NUM_0, &pin_config);
  i2s_start(I2S_NUM_0);
}





void setup() {
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  delay(1000);
  Serial.println("Connecting to WiFi");
#endif

  delay(200);

  WiFiMulti.addAP(ssid, pass);
  while (WiFiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWiFi connected");

  socketIO.begin(serverIP, serverPort);
  socketIO.onEvent(socketIOEvent);






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


DynamicJsonDocument doc(1024);

void collectAndSendAudio() {
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_NUM_0, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);

  if (result == ESP_OK) {
    int samples_read = bytesIn / 2;
    if (samples_read > 0) {
      float mean = 0;

      
      for (int i = 0; i < samples_read; ++i) {
        mean += abs(sBuffer[i]);
        // Serial.println(sBuffer[i]);
     
        if (sBuffer[i] > 200) {
          have_positive = true;
        }
      }
      mean /= samples_read;

      // 如果声音超过阈值，开始录音


      if (mean > threshold && have_positive) {
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

        // Serial.print("Audio data sent with sequence number: ");
        // Serial.println(sequenceNumber - 1);
      }
    }
  }
}

void loop() {
  socketIO.loop();

  if (play_mp3_ready == false) {
    if (strat_init_audio == true) {
      i2s_RX_install();
      strat_init_audio = false;
    }
    collectAndSendAudio();
  } else {

    if (strat_init_audio == false) {
      initURLaudio();
      strat_init_audio = true;
    }
    loopURLaudio();
  }
}
