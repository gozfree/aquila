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

#include "codec.h"
#include "filter.h"

extern struct ikey_cvalue conf_map_table[];

struct venc_ctx {
    struct codec_ctx *encoder;
};

static int on_venc_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct venc_ctx *vc = (struct venc_ctx *)fc->priv;
    int ret = codec_encode(vc->encoder, in, out);
    if (-1 == ret) {
        loge("encode failed!\n");
    }
    return ret;
}

static int venc_open(struct filter_ctx *fc)
{
    struct codec_ctx *encoder = codec_open(fc->url, &fc->media);
    if (!encoder) {
        loge("open codec %s failed!\n", fc->url);
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
    .name     = "vencode",
    .open     = venc_open,
    .on_read  = on_venc_read,
    .on_write = NULL,
    .close    = venc_close,
};