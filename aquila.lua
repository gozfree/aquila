aquila_global = {
        videocap = {
                --type supported: v4l2/vdevfake
                type = "v4l2",
                width = 640,
                height = 480,
        },
        audiocap = {
                type = "alsa",
        },

        vencode = {
                --type supported: mjpeg/x264
                type = "mjpeg",
        },

        playback = {
                -- type supported sdl/snkfake
                type = "sdl",
        },

        filter = {
                graph1 = {
                        source = "videocap",
                        sink = "vencode",
                },
                graph2 = {
                        source = "vencode",
                        sink = "playback",
                },
        },
}

return aquila_global
