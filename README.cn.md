# Aquila

[English](README.md) | 简体中文

[![Build](https://travis-ci.org/gozfree/aquila.svg?branch=master)](https://travis-ci.org/gozfree/aquila)
[![Release](https://img.shields.io/github/release/gozfree/aquila.svg)](https://github.com/gozfree/aquila/releases)
[![License](https://img.shields.io/github/license/gozfree/aquila.svg)](https://github.com/gozfree/aquila/blob/master/LICENSE.MIT)

Aquila 是一套多媒体处理框架，目标是为支持不同厂商的多媒体SDK，并快速推出IPCamera产品而设计的软件框架.
支持软硬件编解码，适用产品：安防视频/家庭监控/行车记录仪/运动DV等



## 代码结构
* `algo`     算法相关：目标检测/识别
* `codec`    音频视频编解码，软件使用ffmpeg，硬件使用vendor SDK
* `device`   音视频硬件适配层，V4L2/PulseAudio/Vendor SDK
* `playback` 回放功能，支持带屏幕的产品 SDL/FrameBuffer/Vendro SDK
* `protocol` 网络协议层，RTMP/RTSP
* `util`     基础组建



## 依赖库
该软件需要依赖 [gear-lib](https://github.com/gozfree/gear-lib)

## 编译
参考 INSTALL.md.

## 寓意
Wiki: Aquila天鹰座，寓意敏捷快速，聪明稳健，视力极佳

## License
参考LICENSE

## 框架简图

```
device ==> encode ==> decode ==> network ==> playback
v4l2       x264       h264       rtsp        sdl
fake       mjpeg                 rtmp
                                 rpc/mqtt
```


## 作者 & 贡献者
非常欢迎参与开发维护这套软件
gozfree <gozfree@163.com>
