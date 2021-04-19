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
#include <gear-lib/liblog.h>

#include "codec.h"
#include "filter.h"

struct videoenc_ctx {
    struct codec_ctx *encoder;
    struct videoenc_conf *conf;
};

static struct iovec *videoenc_alloc(struct filter_ctx *fc)
{
    struct videoenc_ctx *vc = (struct videoenc_ctx *)fc->priv;

    struct media_packet *pkt = media_packet_create(MEDIA_TYPE_VIDEO, NULL, 0);
    if (!pkt) {
        loge("media_packet_create failed\n", pkt);
        return NULL;
    }
    logd("media_packet_create %p\n", pkt);
    struct iovec *io = calloc(1, sizeof(struct iovec));
    io->iov_base = pkt;
    io->iov_len = media_packet_get_size(pkt);
    return io;
}

static void videoenc_free(struct filter_ctx *fc, struct iovec *data)
{
    struct media_packet *pkt = (struct media_packet *)data->iov_base;
    logd("media_packet_destroy %p\n", pkt);
    media_packet_destroy(pkt);
    //free(data);
}

static int on_videoenc_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    int ret;
    struct videoenc_ctx *vc = (struct videoenc_ctx *)fc->priv;
    struct media_packet *pkt = out->iov_base;
    struct iovec iov_pkt;
    iov_pkt.iov_base = pkt->video;
    ret = codec_encode(vc->encoder, in, &iov_pkt);
    if (-1 == ret) {
        loge("encode failed!\n");
    }
    out->iov_len = media_packet_get_size(pkt);
    return ret;
}

static int videoenc_open(struct filter_ctx *fc)
{
    struct codec_ctx *encoder = codec_open(fc->url, &fc->conf->videoenc.me);
    if (!encoder) {
        loge("open codec %s failed!\n", fc->url);
        return -1;
    }
    struct videoenc_ctx *vc = CALLOC(1, struct videoenc_ctx);
    if (!vc) {
        loge("malloc videoenc_ctx failed!\n");
        goto failed;
    }

    vc->conf = &fc->conf->videoenc;
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

static void videoenc_close(struct filter_ctx *fc)
{
    struct videoenc_ctx *vc = (struct videoenc_ctx *)fc->priv;
    if (vc->encoder) {
        codec_close(vc->encoder);
    }
    free(vc);
}

struct filter aq_videoenc_filter = {
    .name       = "videoenc",
    .open       = videoenc_open,
    .on_read    = on_videoenc_read,
    .on_write   = NULL,
    .alloc_data = videoenc_alloc,
    .free_data  = videoenc_free,
    .close      = videoenc_close,
};
