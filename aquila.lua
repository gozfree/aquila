aquila_global = {
        videocap = {
            --array0 = {
                --type = "vdevfake",
                --device = "YUV420P_720x480.yuv",
                --format = "YUV420P",
                --width = 720,
                --height = 480,
            --},
            --array1 = {
                type = "v4l2",
                device = "/dev/video0",
                format = "YUV422P",
                width = 640,
                height = 480,
            --}
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
                width = 720,
                height = 480,
        },
        filter = {
                num = 4,
                type0 = "videocap",
                type1 = "vencode",
                type2 = "vdecode",
                type3 = "playback",
        },

        graph = {
                num = 2,
                graph0 = {
                        source = "videocap",
                        sink = "vencode",
                },
                graph1 = {
                        source = "vencode",
                        sink = "playback",
                },
        },
}

return aquila_global
