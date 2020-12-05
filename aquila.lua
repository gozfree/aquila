aquila_global = {
        videocap = {
                type = "usbcam",
                device = "/dev/video0",
                format = "YUV422P",
                width = 640,
                height = 480,
        },
        audiocap = {
                type = "alsa",
        },

        videoenc = {
                --type supported: mjpeg/x264
                --type = "mjpeg",
                type = "x264",
                format = "YUY2",
                width = 640,
                height = 480,
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

        record = {
                type = "mp4",
                file = "test.mp4",
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
                type = "rtmp",
                port = 554;
        },


        filters = {
                "videocap",
                "videoenc",
                --"vdecode",
                --"playback",
                --"upstream",
                "record",
        },

        graphs = {
                {
                        source = "videocap",
                        sink = "videoenc",
                },
                {
                        source = "videoenc",
                        sink = "record",
                },
        },
}

return aquila_global
