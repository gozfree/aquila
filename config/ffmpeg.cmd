rtmp upstream:
ffmpeg -re -f v4l2 -i /dev/video0 -vcodec libx264  -f flv rtmp://127.0.0.1/live/livestream

generate 264/yuv file from mp4:
ffmpeg -i sample.mp4 -ss 0:0:00 -t 0:0:01  -vcodec h264 -s 320x240 -f m4v sample.264
ffmpeg -i sample.264 -s 320x240 -pix_fmt yuv422p sample_yuv422p.yuv
ffplay -f rawvideo -pix_fmt yuv422p -video_size 320x240 sample_yuv422p.yuv




AVPacket: store compressed data

AVFrame: decoded(raw) data
