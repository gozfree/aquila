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
#include <gear-lib/libmacro.h>
#include <gear-lib/liblog.h>
#include <gear-lib/libatomic.h>

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
    REGISTER_ENCODER(aac);
    REGISTER_ENCODER(mjpeg);
#if 0
    REGISTER_DECODER(h264dec);
    REGISTER_ENCODER(h264enc);
    REGISTER_DECODER(avcodec);
#endif
}

struct codec_ctx *codec_open(const char *url, struct media_encoder *me)
{
    struct codec *p;
    struct codec_ctx *c = CALLOC(1, struct codec_ctx);
    if (!c) {
        loge("malloc codec context failed!\n");
        return NULL;
    }
    if (-1 == url_parse(&c->url, url)) {
        loge("url_parse failed!\n");
        return NULL;
    }
    for (p = first_codec; p != NULL; p = p->next) {
        if (!strcasecmp(c->url.head, p->name))
            break;
    }
    if (p == NULL) {
        loge("%s codec is not support!\n", c->url.head);
        goto failed;
    }
    logi("\t[codec module] <%s> loaded\n", p->name);
    c->ops = p;
    if (!c->ops->open) {
        loge("codec open ops can't be null\n");
        goto failed;
    }
    if (0 != c->ops->open(c, me)) {
        loge("open %s codec failed!\n", c->url.head);
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
