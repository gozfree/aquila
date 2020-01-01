/******************************************************************************
 * Copyright (C) 2014-2018 Zhifeng Gong <gozfree@163.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libraries; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
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

struct venc_ctx {
    struct codec_ctx *encoder;
};

static int on_venc_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct venc_ctx *vc = (struct venc_ctx *)fc->priv;
    struct video_packet *pkt = (struct video_packet*)calloc(1, sizeof(struct video_packet));
    out->iov_base = pkt;
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
    }
    free(vc);
}

struct filter aq_vencode_filter = {
    .name     = "vencode",
    .open     = venc_open,
    .on_read  = on_venc_read,
    .on_write = NULL,
    .close    = venc_close,
};
