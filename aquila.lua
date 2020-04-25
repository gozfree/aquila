aquila_global = {
        videocap = {
                type = "usbcam",
                device = "/dev/video0",
                width = 640,
                height = 480,
                format = "YUY2",
        },
        audiocap = {
                type = "alsa",
        },

        vencode = {
                --type supported: mjpeg/x264
                --type = "mjpeg",
                type = "x264",
                format = "NV12",
        },

        vdecode = {
                type = "h264dec",
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
                "vencode",
                "upstream",
        },

        graphs = {
                {
                        source = "videocap",
                        sink = "vencode",
                },
                {
                        source = "vencode",
                        sink = "upstream",
                },
        },
}

return aquila_global
