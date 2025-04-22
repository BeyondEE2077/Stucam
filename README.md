# Stucam
基于LicheeRV nano实现录像机功能，可拍摄H.264/H.265压缩的视频，生成.mp4文件，并可以通过Web预览和下载视频文件。具有MP3上传并播放功能。还提供一个可3D打印的外壳模型。  

Web效果如下图：  
![web1](https://github.com/user-attachments/assets/6d0c0579-2216-4488-8070-04845658e6d3)

实物照片：  
![stucam](https://github.com/user-attachments/assets/6566405b-0a17-4f2e-a0c7-92cf251a1c00)


## 相关视频
见如下bilibili视频：  
[功能演示](https://www.bilibili.com/video/BV1sy5fztEtK)  
[壳体结构演示](https://www.bilibili.com/video/BV1NB5ezxE8d)  
[本代码使用介绍](https://www.bilibili.com/video/BV1tx5XzjE9W)  

# 本代码使用简介
首先要下载并编译LicheeRV Nano官方系统：  
https://github.com/sipeed/LicheeRV-Nano-Build  


  
随后将如下组件集成到LicheeRV Nano官方系统中，以下按各子目录说明用途和用法：  

### nginx  
autoindex_evilvir.xslt：  
index前端模板文件，这个为nginx调用，配合autoindex模组（ngx_http_autoindex_module）输出视频列表，并提供mp3文件上传的前端接口。
此文件参考源  (https://github.com/EvilVir/Nginx-Autoindex/blob/master/autoindex.xslt)  

nginx.conf：  
nginx配置文件  

部署：  
重配置buildroot并编译nginx，nginx编译选项中要带有ngx_http_autoindex_module和ngx_upload_module两个模组  
将autoindex_evilvir.xslt和nginx.conf放到系统的nginx配置目录（/etc/nginx)  
建立web根目录：/var/www， 并在/var/www建立/v子目录用于存放录像生成的mp4文件。（/usr/html是nginx default site的root path，可将里面的文件从/usr/html拷贝到/var/www）  
建立/var/www/mp3目录，存放上传的mp3文件   

### cam/src  
这里包含录像机（recorder）的程序代码和Makefile。recorder程序使用MMF库实现。  

编译：  
在LicheeRV-Nano-Build源代码的子目录：/middleware/v2/sample/中建立一个recorder子目录，将上述代码和Makefile放入，通过在LicheeRV-Nano-Build目录中运行build_middleware就可以编译。

~~~
source build/cvisetup.sh
defconfig sg2002_licheervnano_sd
build_middleware
~~~
编译后会生成recorder（采用H.264压缩）和recorder_265（采用H.265压缩）两个可执行文件。

部署：  
recorder程序默认安装在目标系统的路径： /mnt/system/usr/bin/  

### user_control
stucam_control.py：   
stucam控制python程序，它读取USER_KEY按键的状态，控制录像/开启和结束，同时配合有LED闪烁频率调整，音效播放。它在随系统开机启动时，也会播放开机音乐。录像调用recorder，录音调用amixer和arecord（这里有个音画延时的问题）。当录像完成时，它调用ffmpeg打包成.mp4文件。  
mp3_watchdog.py：  
mp3控制python程序，它监控是否有mp3上传，一旦有mp3上传完成，就开始播放。  
*.mp3:    
音效资源文件，需要转成.wav才能播放。  

部署：  
将上述.py和.mp3都上传到新建的/usr/share/stucam/目录。  
.mp3要转换成.wav，用目标系统中如下ffmpeg命令：  
~~~
ffmpeg -i general.mp3 -ac 1 -ar 48000 -acodec pcm_s16le gen.wav
~~~

### mech
以下为solidworks文件：  
壳体_LicheeRV_Nano_W_New.SLDPRT：  
壳体零件  
  
button.SLDPRT：  
按钮杆零件  
