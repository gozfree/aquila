/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    videocap_filter.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-04-30 16:02
 * updated: 2016-04-30 16:02
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <liblog.h>

#include "device.h"
#include "filter.h"

#define VIDEOCAP_V4L2       "v4l2:///dev/video0"
#define VIDEOCAP_VDEVFAKE   "vdevfake:////home/gongzf/github/snowball.repo/aquila/720x480.yuv"

#define VIDEOCAP_FILTER_URL VIDEOCAP_V4L2


struct videocap_ctx {
    int seq;
    struct device_ctx *dev;
};

static int on_videocap_read(void *arg, void *in_data, int in_len,
                            void **out_data, int *out_len)
{
    struct videocap_ctx *ctx = (struct videocap_ctx *)arg;
    struct media_params *param = &ctx->dev->media;
    int flen = 2 * (param->video.width) * (param->video.height);//YUV422 2*w*h
    void *frm = calloc(1, flen);

    *out_len = device_read(ctx->dev, frm, flen);
    if (*out_len == -1) {
        loge("device_read failed!\n");
        free(frm);
        if (-1 == device_write(ctx->dev, NULL, 0)) {
            loge("device_write failed!\n");
        }
        return -1;
    }
    if (-1 == device_write(ctx->dev, NULL, 0)) {
        loge("device_write failed!\n");
    }
    *out_data = frm;
    ctx->seq++;
    logv("seq = %d, len = %d\n", ctx->seq, *out_len);
    return *out_len;
}

static char *videocap_probe()
{
#if 0
    char *path = NULL;
    char *buf = NULL;
    if ((buf = get_current_dir_name()) == NULL) {
        loge("getcwd failed: %s\n", strerror(errno));
        return NULL;
    }
    logi("buf: %s\n", buf);
    path = strcat("vdevfake://", buf);
    path = strcat(buf, "/");
    path = strcat(path, VIDEOCAP_VDEVFAKE);
    logi("path: %s\n", path);
    return path;
#else
    return VIDEOCAP_VDEVFAKE;
#endif
}

static int videocap_open(struct filter_ctx *fc)
{
    const char *url = VIDEOCAP_FILTER_URL;
    url = videocap_probe();
    //url = VIDEOCAP_FILTER_URL;
    struct device_ctx *dc = device_open(url);
    if (!dc) {
        loge("open %s failed!\n", url);
        return -1;
    }
    struct videocap_ctx *vc = CALLOC(1, struct videocap_ctx);
    if (!vc) {
        loge("malloc failed!\n");
        goto failed;
    }
    vc->dev = dc;
    vc->seq = 0;

    fc->rfd = vc->dev->fd;
    fc->wfd = -1;
    fc->media.video.width = vc->dev->media.video.width;
    fc->media.video.height = vc->dev->media.video.height;
    fc->priv = vc;
    return 0;

failed:
    if (dc) {
        device_close(dc);
    }
    if (vc) {
        free(vc);
    }
    return -1;
}

static void videocap_close(struct filter_ctx *fc)
{
    struct videocap_ctx *vc = fc->priv;
    if (vc->dev) {
        device_close(vc->dev);
        free(vc);
    }
}

struct filter aq_videocap_filter = {
    .name = "videocap",
    .open = videocap_open,
    .on_read = on_videocap_read,
    .on_write = NULL,
    .close = videocap_close,
};
