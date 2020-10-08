INSTALL
=======

## install lastest libraries
$ `git clone https://github.com/gozfree/libraries.git`  
$ `cd libraries`  
$ `./build.sh`  
$ `sudo ./build.sh install`  

## install essential third-party libraries
  ffmpeg/alsa/x264/sdl  
$ `sudo apt-get install libavformat-dev libavutil-dev libavcodec-dev libswscale-dev
libasound-dev libx264-dev libsdl-dev`

## build aquila
`$ make`

## FAQ
1.error while loading shared libraries: libdebug.so: cannot open shared object file: No such file or directory
  make sure install libraries then
  `$ sudo ldconfig`
ffplay -f rawvideo -pixel_format yuv422p -video_size 320x240 sample/sample_yuv422p.yuv
