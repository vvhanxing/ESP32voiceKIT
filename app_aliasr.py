import ssl
from flask import Flask, render_template
from flask_sock import Sock
import dashscope
from dashscope.audio.asr import *

app = Flask(__name__)
sock = Sock(app)

# 配置 Dashscope API Key
dashscope.api_key = "sk-2b368fe2160f4223a82098770f28df0f"

# 全局变量
translator = None
sentence_dict = {}  # 存储 sentence_id 和最新文本的映射

class Callback(TranslationRecognizerCallback):
    def __init__(self, ws):
        self.ws = ws

    def on_open(self) -> None:
        print("TranslationRecognizerCallback open.")

    def on_close(self) -> None:
        print("TranslationRecognizerCallback close.")
        global sentence_dict
        sentence_dict.clear()  # 关闭时清空字典

    def on_event(
        self,
        request_id,
        transcription_result: TranscriptionResult,
        translation_result: TranslationResult,
        usage,
    ) -> None:
        global sentence_dict
        if transcription_result is not None:
            sentence_id = transcription_result.sentence_id
            text = transcription_result.text
            print(f"Sentence ID: {sentence_id}, Transcription: {text}")

            # 更新 sentence_dict 中的文本
            if sentence_id in sentence_dict:
                if sentence_dict[sentence_id] != text:
                    sentence_dict[sentence_id] = text
                    # 发送更新后的文本和 sentence_id 给前端
                    self.ws.send(f"{sentence_id}||{text}")
            else:
                sentence_dict[sentence_id] = text
                # 首次添加，发送给前端
                self.ws.send(f"{sentence_id}||{text}")

            # 如果有 stash，打印但不发送（仅用于调试）
            if transcription_result.stash is not None:
                print(f"Stash for Sentence ID {sentence_id}: {transcription_result.stash.text}")

@app.route('/')
def index():
    return render_template('index.html')

@sock.route('/audio')
def audio(ws):
    global translator
    translator = TranslationRecognizerRealtime(
        model="gummy-realtime-v1",
        format="pcm",
        sample_rate=16000,
        transcription_enabled=True,
        translation_enabled=False,
        callback=Callback(ws),
    )
    translator.start()
    print("WebSocket 连接已建立，等待音频数据...")

    try:
        while True:
            data = ws.receive()
            if data is None:
                break
            chunk_size = 3200
            for i in range(0, len(data), chunk_size):
                chunk = data[i:i + chunk_size]
                translator.send_audio_frame(chunk)
    except Exception as e:
        print(f"WebSocket 错误: {e}")
    finally:
        translator.stop()
        print("WebSocket 连接关闭")

if __name__ == '__main__':
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(certfile='cert.pem', keyfile='key.pem')
    app.run(host='0.0.0.0', port=5000, ssl_context=context)







# <!DOCTYPE html>
# <html lang="zh-CN">
# <head>
#     <meta charset="UTF-8">
#     <title>实时语音转录</title>
#     <style>
#         #output {
#             width: 100%;
#             height: 300px;
#             border: 1px solid #ccc;
#             overflow-y: auto;
#             padding: 10px;
#         }
#         .sentence {
#             margin: 5px 0;
#         }
#     </style>
# </head>
# <body>
#     <h1>实时语音转录</h1>
#     <button id="startBtn">开始录音</button>
#     <button id="stopBtn" disabled>停止录音</button>
#     <div id="output"></div>

#     <script>
#         let socket;
#         let audioContext;
#         let source;
#         let processor;

#         const startBtn = document.getElementById('startBtn');
#         const stopBtn = document.getElementById('stopBtn');
#         const output = document.getElementById('output');

#         startBtn.onclick = async () => {
#             const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
#             audioContext = new AudioContext({ sampleRate: 16000 });
#             source = audioContext.createMediaStreamSource(stream);
#             processor = audioContext.createScriptProcessor(4096, 1, 1);

#             socket = new WebSocket('wss://localhost:5000/audio');

#             socket.onopen = () => {
#                 console.log('WebSocket 连接已建立');
#                 source.connect(processor);
#                 processor.connect(audioContext.destination);
#                 startBtn.disabled = true;
#                 stopBtn.disabled = false;
#             };

#             socket.onmessage = (event) => {
#                 const [sentenceId, text] = event.data.split('||');
#                 // 查找或创建对应 sentence_id 的元素
#                 let sentenceElement = document.getElementById(`sentence-${sentenceId}`);
#                 if (!sentenceElement) {
#                     sentenceElement = document.createElement('p');
#                     sentenceElement.id = `sentence-${sentenceId}`;
#                     sentenceElement.className = 'sentence';
#                     output.appendChild(sentenceElement);
#                 }
#                 // 更新文本内容
#                 sentenceElement.textContent = text;
#                 output.scrollTop = output.scrollHeight; // 自动滚动到底部
#             };

#             socket.onclose = () => {
#                 console.log('WebSocket 连接关闭');
#             };

#             processor.onaudioprocess = (event) => {
#                 const inputData = event.inputBuffer.getChannelData(0);
#                 const pcmData = new Int16Array(inputData.length);
#                 for (let i = 0; i < inputData.length; i++) {
#                     pcmData[i] = Math.max(-32768, Math.min(32767, inputData[i] * 32768));
#                 }
#                 if (socket.readyState === WebSocket.OPEN) {
#                     socket.send(pcmData.buffer);
#                 }
#             };
#         };

#         stopBtn.onclick = () => {
#             processor.disconnect();
#             source.disconnect();
#             audioContext.close();
#             socket.close();
#             startBtn.disabled = false;
#             stopBtn.disabled = true;
#         };
#     </script>
# </body>
# </html>