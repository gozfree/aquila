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
#ifndef _CODEC_H_
#define _CODEC_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include "common.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

struct codec_ctx {
    struct url url;
    struct codec *ops;
    struct iovec extradata;
    void *priv;
};

struct codec {
    const char *name;
    int (*open)(struct codec_ctx *c, struct media_params *media);
    int (*encode)(struct codec_ctx *c, struct iovec *in, struct iovec *out);
    int (*decode)(struct codec_ctx *c, struct iovec *in, struct iovec *out);
    void (*close)(struct codec_ctx *c);
    struct codec *next;
};

void codec_register_all();
struct codec_ctx *codec_open(const char *name, struct media_params *mediah);
void codec_close(struct codec_ctx *c);
int codec_encode(struct codec_ctx *c, struct iovec *in, struct iovec *out);
int codec_decode(struct codec_ctx *c, struct iovec *in, struct iovec *out);

#ifdef __cplusplus
}
#endif
#endif
