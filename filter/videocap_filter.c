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
#include <liblog.h>
#include <libtime.h>
#include <libmacro.h>
#include <libmedia-io.h>

#include "device.h"
#include "filter.h"
#include "config.h"
#include "overlay.h"

struct videocap_ctx {
    int seq;
    struct device_ctx *dev;
    struct videocap_conf *conf;
};

static int on_videocap_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct videocap_ctx *ctx = (struct videocap_ctx *)fc->priv;
    struct media_params *mp = &fc->mp;
    char tmp_tm[32];
    struct video_frame frm;
    video_frame_init(&frm, mp->video.format, mp->video.width, mp->video.height, VFC_NONE);

    int ret = device_read(ctx->dev, &frm, sizeof(frm));
    if (ret == -1) {
        if (-1 == device_write(ctx->dev, NULL, 0)) {
            loge("device_write failed!\n");
        }
        return -1;
    }

    time_get_msec_str(tmp_tm, sizeof(tmp_tm));
    //overlay_draw_text((unsigned char *)frm, 0, 0, mp->video.width, tmp_tm);
    if (-1 == device_write(ctx->dev, NULL, 0)) {
        loge("device_write failed!\n");
    }
    out->iov_base = video_frame_create(mp->video.format, mp->video.width, mp->video.height, VFC_ALLOC);
    video_frame_copy(out->iov_base, &frm);
    out->iov_len = ret;
    return ret;
}

static int videocap_open(struct filter_ctx *fc)
{
    struct media_params *mp = &fc->mp;
    struct filter_conf *fconf = (struct filter_conf *)fc->config;
    struct videocap_ctx *vc = CALLOC(1, struct videocap_ctx);
    if (!vc) {
        loge("malloc failed!\n");
        return -1;
    }
    vc->conf = &fconf->videocap;
    struct device_ctx *dc = device_open(fc->url, &vc->conf->mp);
    if (!dc) {
        loge("open %s failed!\n", fc->url);
        goto failed;
    }
    memcpy(mp, &vc->conf->mp, sizeof(struct media_params));
    vc->dev = dc;
    vc->seq = 0;

    logi("vc->conf->fomat = %s\n", vc->conf->format);

    fc->rfd = vc->dev->fd;
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
    .name     = "videocap",
    .open     = videocap_open,
    .on_read  = on_videocap_read,
    .on_write = NULL,
    .close    = videocap_close,
};
