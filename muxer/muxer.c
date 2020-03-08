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
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <gear-lib/libatomic.h>
#include <gear-lib/liblog.h>
#include <gear-lib/libmacro.h>


#include "common.h"
#include "muxer.h"

#define REGISTER_MUXER(x)                                                      \
    {                                                                          \
        extern struct muxer aq_##x##_muxer;                                    \
            muxer_register(&aq_##x##_muxer);                                   \
    }


#define REGISTER_DEMUXER(x)                                                    \
    {                                                                          \
        extern struct muxer aq_##x##_demuxer;                                  \
            muxer_register(&aq_##x##_demuxer);                                 \
    }

static struct muxer *first_muxer = NULL;
static struct muxer **last_muxer = &first_muxer;
static int registered = 0;


static void muxer_register(struct muxer *cdc)
{
    struct muxer **p = last_muxer;
    cdc->next = NULL;
    while (*p || atomic_ptr_cas((void * volatile *)p, NULL, cdc))
        p = &(*p)->next;
    last_muxer = &cdc->next;
}

void muxer_register_all()
{
    if (registered)
        return;
    registered = 1;

    REGISTER_MUXER(flv);
#if 0
    REGISTER_MUXER(mp4);
#endif
}


struct muxer_ctx *muxer_open(const char *url, struct media_attr *ma)
{
    struct muxer *p;
    struct muxer_ctx *c = CALLOC(1, struct muxer_ctx);
    if (!c) {
        loge("malloc muxer context failed!\n");
        return NULL;
    }
    if (-1 == url_parse(&c->url, url)) {
        loge("url_parse failed!\n");
        return NULL;
    }
    for (p = first_muxer; p != NULL; p = p->next) {
        if (!strcasecmp(c->url.head, p->name))
            break;
    }
    if (p == NULL) {
        loge("%s muxer is not support!\n", c->url.head);
        goto failed;
    }
    logi("\t[muxer module] <%s> loaded\n", p->name);
    c->ops = p;
    if (!c->ops->open) {
        loge("muxer open ops can't be null\n");
        goto failed;
    }
    if (0 != c->ops->open(c, ma)) {
        loge("open %s muxer failed!\n", c->url.head);
        goto failed;
    }
    return c;

failed:
    if (c) {
        free(c);
    }
    return NULL;
}

int muxer_write_header(struct muxer_ctx *c)
{
    if (!c->ops->write_header) {
        return -1;
    }
    return c->ops->write_header(c);
}

int muxer_write_packet(struct muxer_ctx *c, struct media_packet *pkt)
{
    if (!c->ops->write_packet) {
        return -1;
    }
    return c->ops->write_packet(c, pkt);
}

int muxer_write_tailer(struct muxer_ctx *c)
{
    if (!c->ops->write_tailer) {
        return -1;
    }
    return c->ops->write_tailer(c);
}

int muxer_read_header(struct muxer_ctx *c)
{
    if (!c->ops->read_header) {
        return -1;
    }
    return c->ops->read_header(c);
}

int muxer_read_packet(struct muxer_ctx *c, struct media_packet *pkt)
{
    if (!c->ops->read_packet) {
        return -1;
    }
    return c->ops->read_packet(c, pkt);
}

int muxer_read_tailer(struct muxer_ctx *c)
{
    if (!c->ops->read_tailer) {
        return -1;
    }
    return c->ops->read_tailer(c);
}

void muxer_close(struct muxer_ctx *c)
{
    if (!c) {
        return;
    }
    if (c->ops->close) {
        c->ops->close(c);
    }
    free(c);
}
