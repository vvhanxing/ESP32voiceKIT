# ESP32voiceKIT
ESP32voiceKIT api and script



## 安装配置 nginx
```
sudo apt update
apt install nginx
cd //etc/nginx/
vim nginx.conf

systemctl start nginx
nginx -s reload
```
## 安装miniconde
```
wget https://mirrors.tuna.tsinghua.edu.cn/anaconda/miniconda/Miniconda3-py38_4.8.3-Linux-x86_64.sh --no-check-certificate  
sh Miniconda3-py38_4.8.3-Linux-x86_64.sh  
vim ~/.bashrc   

export PATH="/root/miniconda3/bin:$PATH"  
source ~/.bashrc

pip install flask 
pip install dashscope 

```
## 安装ffmpeg
```
sudo apt install ffmpeg  
```

## 安装阿里云服务sdk 
```
git clone https://github.com/aliyun/alibabacloud-nls-python-sdk.git 
# githubfast镜像：https://githubfast.com/ 
cd alibabacloud-nls-python-sdk 
pip install -r requirements.txt 
pip install . 
```

## 安装TUMX
```
sudo apt install tmux
tmux
tmux attach

```






```python

import os
from openai import OpenAI

client = OpenAI(
    api_key="sk-2b368fe2160f4223a82098770f28df0f",  # 如果您没有配置环境变量，请在此处用您的API Key进行替换
    base_url="https://dashscope.aliyuncs.com/compatible-mode/v1"  # 百炼服务的base_url
)

completion = client.embeddings.create(
    model="text-embedding-v3",
    input='The clothes are of good quality and look good, definitely worth the wait. I love them.',
    dimensions=1024,
    encoding_format="float"
)

print(completion.model_dump_json())


```
