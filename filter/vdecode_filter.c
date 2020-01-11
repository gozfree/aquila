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

#include <libmacro.h>
#include <liblog.h>

#include "filter.h"
#include "common.h"
#include "codec.h"

struct vdec_ctx {
    struct codec_ctx *decoder;
};

static int on_vdec_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct vdec_ctx *vc = (struct vdec_ctx *)fc->priv;
    int ret = codec_decode(vc->decoder, in, out);
    if (ret == -1) {
        loge("decode failed!\n");
    }
    out->iov_len = ret;
    return ret;
}

static int vdec_open(struct filter_ctx *fc)
{
    struct codec_ctx *decoder = codec_open(fc->url, &fc->mp);
    if (!decoder) {
        loge("open codec %s failed!\n", fc->url);
        return -1;
    }
    struct vdec_ctx *vc = CALLOC(1, struct vdec_ctx);
    if (!vc) {
        loge("malloc vdec_ctx failed!\n");
        goto failed;
    }
    vc->decoder = decoder;
    fc->rfd = -1;
    fc->wfd = -1;
    fc->priv = vc;
    return 0;
failed:
    if (decoder) {
        codec_close(decoder);
    }
    if (vc) {
        free(vc);
    }
    return -1;
}

static void vdec_close(struct filter_ctx *fc)
{
    struct vdec_ctx *c = (struct vdec_ctx *)fc->priv;
    if (c->decoder) {
        codec_close(c->decoder);
        free(c->decoder);
    }
}

struct filter aq_vdecode_filter = {
    .name     = "vdecode",
    .open     = vdec_open,
    .on_read  = on_vdec_read,
    .on_write = NULL,
    .close    = vdec_close,
};
