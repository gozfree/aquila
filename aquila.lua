aquila_global = {
        videocap = {
            --array0 = {
                --type = "vdevfake",
                --device = "720x480.yuv",
                --format = "YUV420",
                --width = 720,
                --height = 480,
            --},
            --array1 = {
                type = "v4l2",
                device = "/dev/video0",
                format = "YUV422",
                width = 640,
                height = 480,
            --}
        },
        audiocap = {
                type = "alsa",
        },

        vencode = {
                --type supported: mjpeg/x264
                type = "mjpeg",
                --type = "x264",
        },

        playback = {
                -- type supported sdl/snkfake
                --type = "snkfake",
                --format = "file",
                type = "sdl",
                format = "rgb",
                width = 640,
                height = 480,
        },
        filter = {
                num = 2,
                type0 = "videocap",
                --type1 = "vencode",
                type1 = "playback",
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
