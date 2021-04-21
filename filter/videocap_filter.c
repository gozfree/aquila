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
#include <gear-lib/liblog.h>
#include <gear-lib/libtime.h>
#include <gear-lib/libmedia-io.h>

#include "device.h"
#include "filter.h"
#include "config.h"
#include "overlay.h"

struct videocap_ctx {
    int seq;
    struct device_ctx *dev;
    struct videocap_conf *conf;
};

static struct iovec *videocap_alloc(struct filter_ctx *fc)
{
    struct videocap_ctx *ctx = (struct videocap_ctx *)fc->priv;
    struct media_producer *mp = &fc->conf->videocap.mp;
    struct video_frame *frm = video_frame_create(mp->video.format, mp->video.width, mp->video.height, MEDIA_MEM_DEEP);
    if (!frm) {
        loge("video_frame_create failed!\n");
        return NULL;
    }
    logd("video_frame_create %p\n", frm);
    struct iovec *io = calloc(1, sizeof(struct iovec));
    io->iov_base = frm;
    io->iov_len = frm->total_size;
    return io;
}

static void videocap_free(struct filter_ctx *fc, struct iovec *data)
{
    struct video_frame *frm = (struct video_frame *)data->iov_base;
    video_frame_destroy(frm);
    logd("video_frame_destroy %p\n", frm);
    //free(data);
}

static int on_videocap_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct videocap_ctx *ctx = (struct videocap_ctx *)fc->priv;
    struct media_producer *mp = &fc->conf->videocap.mp;
    char tmp_tm[32];
    struct video_frame *frm = out->iov_base;
    if (!frm) {
        loge("on_videocap_read input failed!\n");
        return -1;
    }

    int ret = device_read(ctx->dev, frm, sizeof(struct video_frame));
    if (ret == -1) {
        if (-1 == device_write(ctx->dev, NULL, 0)) {
            loge("device_write failed!\n");
        }
        return -1;
    }

    time_msec_str(tmp_tm, sizeof(tmp_tm));
    //overlay_draw_text((unsigned char *)frm, 0, 0, mp->video.width, tmp_tm);
    if (-1 == device_write(ctx->dev, NULL, 0)) {
        loge("device_write failed!\n");
    }
    out->iov_len = frm->total_size;
    return ret;
}

static int videocap_open(struct filter_ctx *fc)
{
    struct device_ctx *dc = device_open(fc->url, &fc->conf->videocap.mp);
    if (!dc) {
        loge("open %s failed!\n", fc->url);
        return -1;
    }
    struct videocap_ctx *vc = CALLOC(1, struct videocap_ctx);
    if (!vc) {
        loge("malloc videocap_ctx failed!\n");
        goto failed;
    }

    vc->conf = &fc->conf->videocap;
    vc->dev = dc;
    vc->seq = 0;

    fc->rfd = dc->fd;
    fc->wfd = -1;
    fc->priv = vc;
    overlay_init();
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
    struct videocap_ctx *vc = (struct videocap_ctx *)fc->priv;
    if (vc->dev) {
        device_close(vc->dev);
        free(vc);
    }
}

struct filter aq_videocap_filter = {
    .name       = "videocap",
    .open       = videocap_open,
    .on_read    = on_videocap_read,
    .on_write   = NULL,
    .alloc_data = videocap_alloc,
    .free_data  = videocap_free,
    .close      = videocap_close,
};
