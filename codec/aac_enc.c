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
#include <gear-lib/libmacro.h>
#include <gear-lib/liblog.h>
#include <gear-lib/libdarray.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "codec.h"
#include "common.h"

struct aac_ctx {
    void *parent;
};

static int aac_open(struct codec_ctx *cc, struct media_encoder *ma)
{
    struct aac_ctx *c = CALLOC(1, struct aac_ctx);
    if (!c) {
        loge("malloc aac_ctx failed!\n");
        return -1;
    }

    c->parent = cc;
    cc->priv = c;
    return 0;
}

static int aac_encode(struct codec_ctx *cc, struct iovec *in, struct iovec *out)
{
    return 0;
}

static void aac_close(struct codec_ctx *cc)
{
    struct aac_ctx *c = (struct aac_ctx *)cc->priv;
    free(c);
}

struct codec aq_aac_encoder = {
    .name   = "aac",
    .open   = aac_open,
    .encode = aac_encode,
    .decode = NULL,
    .close  = aac_close,
};
