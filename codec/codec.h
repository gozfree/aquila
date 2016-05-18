/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    codec.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-04 00:50
 * updated: 2016-05-04 00:50
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
