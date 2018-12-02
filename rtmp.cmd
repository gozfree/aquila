ffmpeg -re -f v4l2 -i /dev/video0 -vcodec libx264  -f flv rtmp://127.0.0.1/live/livestream

AVPacket: store compressed data

AVFrame: decoded(raw) data
