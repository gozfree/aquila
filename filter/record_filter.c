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
#include <unistd.h>
#include <errno.h>
#include <assert.h>

#include <gear-lib/liblog.h>

#include "filter.h"
#include "common.h"
#include "muxer.h"

struct rec_ctx {
    struct muxer_ctx *muxer;
};

static int on_rec_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct media_packet *pkt;
    struct rec_ctx *vc = (struct rec_ctx *)fc->priv;
    pkt = in->iov_base;
    int ret = muxer_write_packet(vc->muxer, pkt);
    if (ret == -1) {
        loge("decode failed!\n");
    }
    return ret;
}

static int rec_open(struct filter_ctx *fc)
{
    memcpy(&fc->conf->record.me, &fc->prev_conf->videoenc.me, sizeof(struct media_encoder));
    struct muxer_ctx *muxer = muxer_open(fc->url, &fc->conf->record.me);
    if (!muxer) {
        loge("open muxer %s failed!\n", fc->url);
        return -1;
    }
    struct rec_ctx *vc = CALLOC(1, struct rec_ctx);
    if (!vc) {
        loge("malloc rec_ctx failed!\n");
        goto failed;
    }
    vc->muxer = muxer;
    fc->rfd = -1;
    fc->wfd = -1;
    fc->priv = vc;
    return 0;
failed:
    if (muxer) {
        muxer_close(muxer);
    }
    if (vc) {
        free(vc);
    }
    return -1;
}

static void rec_close(struct filter_ctx *fc)
{
    struct rec_ctx *c = (struct rec_ctx *)fc->priv;
    if (c->muxer) {
        muxer_close(c->muxer);
        free(c);
    }
}

struct filter aq_record_filter = {
    .name     = "record",
    .open     = rec_open,
    .on_read  = on_rec_read,
    .on_write = NULL,
    .close    = rec_close,
};
