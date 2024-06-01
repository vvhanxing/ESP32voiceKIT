
from flask import Flask, request, send_file
import os
import subprocess

import wave

import requests
import json
import aliyunapi
import shutil
from http import HTTPStatus
import dashscope
dashscope.api_key = "sk-80d6d634c5514cdca77bc6ae448d8912"

def llm_pridect(prompt,model="ailiyun"):#"dify"
    print("prompt:",prompt)
    print("type",type(prompt))
    if prompt==None:
        prompt = "主人来啦"
    if model=="dify":
        url = 'https://api.dify.ai/v1/workflows/run'
        headers = {
            'Authorization': 'Bearer app-TIfJY5lsgtBP74niv58HUBa2',
            'Content-Type': 'application/json'
        }
        data = {
            "inputs": {"sysPrompt":"假如你是一个可爱的女孩,愿意回答用户的任何问题","prompt":prompt},
            "response_mode": "blocking",
            "user": "abc-1"
        }

        response = requests.post(url, headers=headers, json=data)

        print(response.json())
        return response.json()["data"]["outputs"]["result"]
    if model=="ailiyun":

        messages = [{'role': 'system', 'content': '假如你是一个可爱的女孩,愿意回答用户的任何问题'},
                    {'role': 'user', 'content': prompt}]

        response = dashscope.Generation.call(
            dashscope.Generation.Models.qwen_turbo,
            messages=messages,
            result_format='message',  # 将返回结果格式设置为 message
        )
        if response.status_code == HTTPStatus.OK:
        # print(response)
            result = response.output.choices[0].message.content
            print(result )
            return  result 
        else:
            print('Request id: %s, Status code: %s, error code: %s, error message: %s' % (
                response.request_id, response.status_code,
                response.code, response.message
            ))
            return "嗯哼，我脑袋空空"        




def copy_file(input_path, output_path):
    # 检查输入文件是否存在
    if not os.path.exists(input_path):
        print("输入文件不存在！")
        return
    
    # 复制文件到新路径
    try:
        shutil.copy(input_path, output_path)
        print("文件已成功复制到:", output_path)
    except Exception as e:
        print("复制文件时出错:", e)
def get_speech_(text):
    # 定义要发送的文本数据
    aliyunapi.tts()

def get_speech(text):

    aliyunapi.tts(text)


def ffmpeg_WavToMP3(input_path, output_path):
    # 提取input_path路径下所有文件名
    filename = os.listdir(input_path)
    for file in filename:
        if ".wav" in file and "output.wav" ==file:
            path1 = input_path + "/" + file
            path2 = output_path + "/" + os.path.splitext(file)[0]
            cmd = "ffmpeg -i " + path1 + " -b:a 128k " + path2 + ".mp3 -y" #将input_path路径下所有音频文件转为.mp3文件
            print(cmd)
            os.system(cmd)

input_path = r"./"
output_path = r"./"

app = Flask(__name__)
audio_data = b""


@app.route('/')
def sayhi():
    return "api running"

@app.route('/audio2text')
def audio2text():
    #return "success"
    global audio_data
    audio_data = b""

    stt_text =  aliyunapi.stt()
  
    print("stt_text:",stt_text)
    prompt = stt_text
    llm_answer = llm_pridect(prompt)
    get_speech(llm_answer)
    ffmpeg_WavToMP3(".",".")
    print("ffmpeg_WavToMP3 success")
    return "start speaking"


@app.route('/audio/mp3')
def stream_mp3():
    
    global audio_data

    audio_data = b""
    audio_file = 'output.mp3'
    # ffmpeg_MP3ToWav(input_path, output_path)
    return send_file(audio_file, mimetype='audio/mpeg')


@app.route('/record', methods=['POST'])
def upload_file():
    global audio_data
    try:
        # 从请求中获取二进制数据
        audio_data += request.data
        #print(len(audio_data))

        # 将二进制数据写入.wav文件
        with wave.open('./uploaded_audio.wav', 'wb') as wf:
            wf.setnchannels(1)  # 单声道
            wf.setsampwidth(2)  # 16位采样
            wf.setframerate(int(16000))  # 采样率为16000Hz
            wf.writeframes(audio_data)

        return  'Audio uploaded successfully'
    except Exception as e:
        print(e)
        return 'error'

if __name__ == '__main__':
    app.run(host="localhost",port=5000,debug=True)
