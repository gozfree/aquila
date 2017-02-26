aquila_global = {
        videocap = {
                type = "v4l2",
                device = "/dev/video0",
                format = "YUV422P",
                width = 640,
                height = 480,
        },
        audiocap = {
                type = "alsa",
        },

        vencode = {
                --type supported: mjpeg/x264
                --type = "mjpeg",
                type = "x264",
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
		port = 554;
        },


        filter = {
                num = 4,
                type0 = "videocap",
                type1 = "vencode",
                type2 = "vdecode",
                --type3 = "playback",
                type3 = "upstream",
        },

        graph = {
                num = 2,
                graph0 = {
                        source = "videocap",
                        sink = "vencode",
                },
                graph1 = {
                        source = "vencode",
                        sink = "upstream",
                },
        },
}

return aquila_global
