aquila_global = {
        videocap = {
                type = "usbcam",
                device = "/dev/video0",
                width = 640,
                height = 480,
                format = "YUY2",
        },
        audiocap = {
                type = "pulseaudio",
                device = "alsa_input.pci-0000_00_1b.0.analog-stereo",
                sample_rate = 44100,
                channels = 2,
                format = "PCM_S16LE"
        },
        videoenc = {
                --type supported: mjpeg/x264
                --type = "mjpeg",
                type = "x264",
                format = "NV12",
        },
        audioenc = {
                --type supported: aac/speex
                type = "aac",
        },
        videodec = {
                type = "h264dec",
        },

        aencode = {
                type = "aac",
        },

        overlay = {
                type = "timestamp",
                offsetx = 0,
                offsety = 0,
                switch = "on",
        },

        playback = {
                -- type supported sdl/snkfake
                --type = "snkfake",
                --format = "file",
                type = "sdl",
                --format = "rgb",
                format = "yuv",
                width = 640,
                height = 480,
        },

        upstream = {
                type = "rtsp",
                port = 8554,
                url = "rtsp://localhost/usbcam",
        },


        filters = {
                "videocap",
                "videoenc",
                "upstream",
        },

        graphs = {
                {
                        source = "videocap",
                        sink = "videoenc",
                },
                {
                        source = "videoenc",
                        sink = "upstream",
                },
        },
}

return aquila_global
