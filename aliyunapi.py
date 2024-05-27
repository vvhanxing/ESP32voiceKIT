


# -*- coding: UTF-8 -*-
# Python 2.x引入httplib模块。
# import httplib
# Python 3.x引入http.client模块。
import http.client
# Python 2.x引入urllib模块。
# import urllib
# Python 3.x引入urllib.parse模块。
import urllib.parse
import json

#! /usr/bin/env python
# coding=utf-8
import os
import time
import json
from aliyunsdkcore.client import AcsClient
from aliyunsdkcore.request import CommonRequest

# 创建AcsClient实例
client = AcsClient(
   "LTAI5t7D3MNPNb6EJASM6qB",#os.getenv('ALIYUN_AK_ID6'),
   "NGEvoIkwWlVIlsJcMcgz4AZlrfkl8",#os.getenv('ALIYUN_AK_SECRETn'),
   "cn-shanghai"
)

expireTime = 0
def getToken():
    global expireTime
    if expireTime<100:
        # 创建request，并设置参数。
        request = CommonRequest()
        request.set_method('POST')
        request.set_domain('nls-meta.cn-shanghai.aliyuncs.com')
        request.set_version('2019-02-28')
        request.set_action_name('CreateToken')

        try : 
            response = client.do_action_with_exception(request)
            print(response)

            jss = json.loads(response)
            if 'Token' in jss and 'Id' in jss['Token']:
                token = jss['Token']['Id']
                expireTime = jss['Token']['ExpireTime']
                print("expireTime = " + str(expireTime))
                print("token = " + token)
                return token
                
        except Exception as e:
            print(e)




def processGETRequest(appKey, token, text, audioSaveFile, format, sampleRate) :
    host = 'nls-gateway-cn-shanghai.aliyuncs.com'
    url = 'https://' + host + '/stream/v1/tts'
    # 设置URL请求参数
    url = url + '?appkey=' + appKey
    url = url + '&token=' + token
    url = url + '&text=' + text
    url = url + '&format=' + format
    url = url + '&sample_rate=' + str(sampleRate)
    # voice 发音人，可选，默认是xiaoyun。
    # url = url + '&voice=' + 'xiaoyun'
    # volume 音量，范围是0~100，可选，默认50。
    # url = url + '&volume=' + str(50)
    # speech_rate 语速，范围是-500~500，可选，默认是0。
    # url = url + '&speech_rate=' + str(0)
    # pitch_rate 语调，范围是-500~500，可选，默认是0。
    # url = url + '&pitch_rate=' + str(0)
    #print(url)
    # Python 2.x请使用httplib。
    # conn = httplib.HTTPSConnection(host)
    # Python 3.x请使用http.client。
    conn = http.client.HTTPSConnection(host)
    conn.request(method='GET', url=url)
    # 处理服务端返回的响应。
    response = conn.getresponse()
    print('Response status and response reason:')
    print(response.status ,response.reason)
    contentType = response.getheader('Content-Type')
    print(contentType)
    body = response.read()
    if 'audio/mpeg' == contentType :
        with open(audioSaveFile, mode='wb') as f:
            f.write(body)
        print('The GET request succeed!')
    else :
        print('The GET request failed: ' + str(body))
    conn.close()



def tts(text):
    appKey = 'neOKFzoQEin4ykLg'
    token =getToken()# '58796110d1814e7487f9f3ae9c6ffd4d'
    
    # 采用RFC 3986规范进行urlencode编码。
    textUrlencode = text
    # Python 2.x请使用urllib.quote。
    # textUrlencode = urllib.quote(textUrlencode, '')
    # Python 3.x请使用urllib.parse.quote_plus。
    textUrlencode = urllib.parse.quote_plus(textUrlencode)
    textUrlencode = textUrlencode.replace("+", "%20")
    textUrlencode = textUrlencode.replace("*", "%2A")
    textUrlencode = textUrlencode.replace("%7E", "~")
    #print('text: ' + textUrlencode)
    audioSaveFile = 'output.wav'
    format = 'wav'
    sampleRate = 16000
    # GET请求方式
    processGETRequest(appKey, token, textUrlencode, audioSaveFile, format, sampleRate)
    # POST请求方式
    # processPOSTRequest(appKey, token, text, audioSaveFile, format, sampleRate)
    return True







import time
import threading
import sys

import nls
import json

URL="wss://nls-gateway-cn-shanghai.aliyuncs.com/ws/v1"
TOKEN=getToken()#"58796110d1814e7487f9f3ae9c6ffd4d"   #参考https://help.aliyun.com/document_detail/450255.html获取token
APPKEY="neOKFzoQEin4ykLg"      #获取Appkey请前往控制台：https://nls-portal.console.aliyun.com/applist

#以下代码会根据音频文件内容反复进行一句话识别
class TestSr:
    def __init__(self, tid, test_file):
        self.__th = threading.Thread(target=self.__test_run)
        self.__id = tid
        self.__test_file = test_file
        self.result = ""
   
    def loadfile(self, filename):
        with open(filename, "rb") as f:
            self.__data = f.read()
    
    def start(self):
        self.loadfile(self.__test_file)
        self.__th.start()

    def test_on_start(self, message, *args):
        print("test_on_start:{}".format(message))
        self.result = ""
    def test_on_error(self, message, *args):
        print("on_error args=>{}".format(args))

    def test_on_close(self, *args):
        print("on_close: args=>{}".format(args))

    def test_on_result_chg(self, message, *args):
        print("test_on_chg:{}".format(message))

    def test_on_completed(self, message, *args):
        print("on_completed:args=>{} message=>{}".format(args, message))
        self.result =message
        #print("self.result ",self.result )


    def __test_run(self):
        print("thread:{} start..".format(self.__id))
        
        sr = nls.NlsSpeechRecognizer(
                    url=URL,
                    token=TOKEN,
                    appkey=APPKEY,
                    on_start=self.test_on_start,
                    on_result_changed=self.test_on_result_chg,
                    on_completed=self.test_on_completed,
                    on_error=self.test_on_error,
                    on_close=self.test_on_close,
                    callback_args=[self.__id]
                )
        
        print("{}: session start".format(self.__id))
        r = sr.start(aformat="wav", ex={"hello":123})
        self.__slices = zip(*(iter(self.__data),) * 640)
        for i in self.__slices:
            sr.send_audio(bytes(i))
            time.sleep(0.01)
      
        r = sr.stop()
        #print("{}: sr stopped:{}".format(self.__id, r))
        #print(dir(sr))
        
        time.sleep(1)

def multiruntest(file_name = "uploaded_audio.wav",num=1):

    name = "thread" + str(0)
    t = TestSr(name,file_name )
    t.start()
    #print("===============>",t.result)
    
    return t
import json


def stt():
    # 设置打开日志输出
    nls.enableTrace(False)
    t = multiruntest()
    for i in range(10):
        if t.result!="":
            return  json.loads(t.result)["payload"]["result"]
        else :
            time.sleep(0.5)




if __name__=="__main__":
    text = '哇哦，今天又周末了,好开心呀'
    tts(text)
    text = stt()
    print(text)

