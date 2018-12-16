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
#include <libmacro.h>
#include <liblog.h>
#include <libatomic.h>

#include "common.h"
#include "protocol.h"

#define REGISTER_PROTOCOL(x)                                                   \
    {                                                                          \
        extern struct protocol aq_##x##_protocol;                              \
            protocol_register(&aq_##x##_protocol);                             \
    }


static struct protocol *first_protocol = NULL;
static struct protocol **last_protocol = &first_protocol;
static int registered = 0;

static void protocol_register(struct protocol *proto)
{
    struct protocol **p = last_protocol;
    proto->next = NULL;
    while (*p || atomic_ptr_cas((void * volatile *)p, NULL, proto))
        p = &(*p)->next;
    last_protocol = &proto->next;
}

void protocol_register_all()
{
    if (registered)
        return;
    registered = 1;

    REGISTER_PROTOCOL(rtmp);
    REGISTER_PROTOCOL(rtsp);
    REGISTER_PROTOCOL(rtp);
    REGISTER_PROTOCOL(rpcd);
}

struct protocol_ctx *protocol_open(const char *url, struct media_params *media)
{
    struct protocol *p;
    struct protocol_ctx *pc = CALLOC(1, struct protocol_ctx);
    if (!pc) {
        loge("malloc protocol context failed!\n");
        return NULL;
    }
    if (-1 == url_parse(&pc->url, url)) {
        loge("url_parse failed!\n");
        return NULL;
    }

    for (p = first_protocol; p != NULL; p = p->next) {
        if (!strcasecmp(pc->url.head, p->name))
            break;
    }
    if (p == NULL) {
        loge("%s protocol is not support!\n", pc->url.head);
        goto failed;
    }
    logi("\t[protocol module] <%s> %s loaded\n", p->name, pc->url.head);

    pc->ops = p;
    if (!pc->ops->open) {
        loge("protocol open ops can't be null\n");
        goto failed;
    }
    if (0 != pc->ops->open(pc, url, media)) {
        loge("open %s failed!\n", url);
        goto failed;
    }
    return pc;
failed:
    if (pc) {
        free(pc);
    }
    return NULL;
}

int protocol_read(struct protocol_ctx *c, void *buf, int len)
{
    if (!c->ops->read)
        return -1;
    return c->ops->read(c, buf, len);
}

int protocol_write(struct protocol_ctx *c, void *buf, int len)
{
    if (!c->ops->write)
        return -1;
    return c->ops->write(c, buf, len);
}

void protocol_close(struct protocol_ctx *c)
{
    if (!c) {
        return;
    }
    if (c->ops->close) {
        c->ops->close(c);
    }
    free(c);
}

