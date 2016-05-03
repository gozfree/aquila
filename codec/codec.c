/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    codec.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-04 00:37
 * updated: 2016-05-04 00:37
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libgzf.h>
#include <liblog.h>
#include <libatomic.h>

#include "common.h"
#include "codec.h"

#define REGISTER_ENCODER(x)                                                    \
    {                                                                          \
        extern struct codec aq_##x##_encoder;                                  \
            codec_register(&aq_##x##_encoder);                                 \
    }


#define REGISTER_DECODER(x)                                                    \
    {                                                                          \
        extern struct codec aq_##x##_decoder;                                  \
            codec_register(&aq_##x##_decoder);                                 \
    }

static struct codec *first_codec = NULL;
static struct codec **last_codec = &first_codec;
static int registered = 0;


static void codec_register(struct codec *cdc)
{
    struct codec **p = last_codec;
    cdc->next = NULL;
    while (*p || atomic_ptr_cas((void * volatile *)p, NULL, cdc))
        p = &(*p)->next;
    last_codec = &cdc->next;
}

void codec_register_all()
{
    if (registered)
        return;
    registered = 1;

    REGISTER_ENCODER(x264);
    REGISTER_ENCODER(mjpeg);
#if 0
    REGISTER_DECODER(avcodec);
#endif
}

struct codec_ctx *codec_open(const char *name, int width, int height)
{
    struct codec *p;
    struct codec_ctx *c = CALLOC(1, struct codec_ctx);
    if (!c) {
        loge("malloc codec context failed!\n");
        return NULL;
    }
    for (p = first_codec; p != NULL; p = p->next) {
        if (!strcmp(name, p->name))
            break;
    }
    if (p == NULL) {
        loge("%s codec is not support!\n", name);
        goto failed;
    }
    logi("codec module %s loaded\n", p->name);
    c->ops = p;
    if (!c->ops->open) {
        loge("codec open ops can't be null\n");
        goto failed;
    }
    if (0 != c->ops->open(c, width, height)) {
        loge("open %s codec failed!\n", name);
        goto failed;
    }
    return c;

failed:
    if (c) {
        free(c);
    }
    return NULL;
}

int codec_encode(struct codec_ctx *c, struct iovec *in, struct iovec *out)
{
    if (!c->ops->encode) {
        return -1;
    }
    return c->ops->encode(c, in, out);
}

int codec_decode(struct codec_ctx *c, struct iovec *in, struct iovec *out)
{
    if (!c->ops->decode)
        return -1;
    return c->ops->decode(c, in, out);
}

void codec_close(struct codec_ctx *c)
{
    if (!c) {
        return;
    }
    if (c->ops->close) {
        c->ops->close(c);
    }
    free(c);
}
