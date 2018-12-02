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
                type = "rtmp",
		url = "rtmp://send3.douyu.com/live/5864578rN1HM8geH?wsSecret=a67935210b620acb5561bb7dfbe2e9fb&wsTime=5bdee212&wsSeek=off&wm=0&tw=0",
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
