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

pip install flask 
pip install dashscope 


export PATH="/root/miniconda3/bin:$PATH"  
source ~/.bashrc  
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
pip install -r requments.txt 
pip install . 
```
