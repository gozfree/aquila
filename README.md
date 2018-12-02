Aquila
======

Aquila is an app-level framework to process multimedia, aims to unify the
different middleware SDK on generic level. It mainly support software encoding
and decoding on CPU, and easily porting. It can be used on video surveillance,
ipcam or drone.

## Framework
* `algo`     algorithm
* `codec`    video enc/dec codecs
* `device`   video, audio and other multimedia source devices
* `playback` video, audio and other multimedia sink devices
* `protocol` network protocols
* `util`     utility

## Libraries
This app is mostly based on [libraries](https://github.com/gozfree/libraries)

## Build
How to build aquila, please refer to INSTALL.md.

## Documentation
The documentation is available in the **doc/** directory.

## Moral
Wiki: Aquila is the Latin and Romance languages word for eagle.
Meaning fast, robust, intelligent, and good eyesight.

## License
Aquila codebase is mainly LGPL-licensed with optional components licensed under
GPL. Please refer to the LICENSE file for detailed information.

## Usage

generate 264/yuv file from mp4

ffmpeg -i sample.mp4 -ss 0:0:00 -t 0:0:01  -vcodec h264 -s 320x240 -f m4v sample.264
ffmpeg -i sample.264 -s 320x240 -pix_fmt yuv422p sample_yuv422p.yuv
ffplay -f rawvideo -pix_fmt yuv422p -video_size 320x240 sample_yuv422p.yuv

## Framework

device ==> encode ==> decode ==> network ==> playback
v4l2       x264       h264       rtsp        sdl
fake       mjpeg                 rtmp




## Author & Contributing
Welcome pull request to the project.  
gozfree <gozfree@163.com>
