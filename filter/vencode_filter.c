/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    mjpegenc_filter.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-05 22:53
 * updated: 2016-05-05 22:53
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <liblog.h>

#include "queue.h"
#include "codec.h"
#include "filter.h"

#define CODEC_FILTER_ENC_MJPEG  "mjpeg"

struct venc_ctx {
    struct codec_ctx *encoder;
};

static int on_venc_read(void *arg, void *in_data, int in_len, void **out_data, int *out_len)
{
    struct venc_ctx *xa = (struct venc_ctx *)arg;
    struct codec_ctx *encoder = xa->encoder;
    struct iovec in, out;
    in.iov_base = in_data;
    in.iov_len = in_len;
    *out_len = codec_encode(encoder, &in, &out);
    if (*out_len == -1) {
        loge("encode failed!\n");
    }
    *out_data = out.iov_base;
    *out_len = out.iov_len;

    //logd("%s:%d len = %d\n", __func__, __LINE__, *out_len);
    return *out_len;
}

static int venc_open(struct filter_ctx *fc)
{
    const char *name = CODEC_FILTER_ENC_MJPEG;
    int width = fc->media.video.width;
    int height = fc->media.video.height;
    struct codec_ctx *encoder = codec_open(name, width, height);
    if (!encoder) {
        loge("open codec %s failed!\n", name);
        return -1;
    }
    struct venc_ctx *vc = CALLOC(1, struct venc_ctx);
    if (!vc) {
        loge("malloc venc_ctx failed!\n");
        goto failed;
    }
    vc->encoder = encoder;
    fc->rfd = -1;
    fc->wfd = -1;
    fc->priv = vc;
    return 0;
failed:
    if (encoder) {
        codec_close(encoder);
    }
    if (vc) {
        free(vc);
    }
    return -1;
}

static void venc_close(struct filter_ctx *fc)
{
    struct venc_ctx *vc = (struct venc_ctx *)fc->priv;
    if (vc->encoder) {
        codec_close(vc->encoder);
        free(vc->encoder);
    }
}

struct filter aq_vencode_filter = {
    .name = "vencode",
    .open = venc_open,
    .on_read = on_venc_read,
    .on_write = NULL,
    .close = venc_close,
};
