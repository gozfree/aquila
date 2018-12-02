/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    protocol.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-22 22:17
 * updated: 2016-05-22 22:17
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

    REGISTER_PROTOCOL(rtsp);
    REGISTER_PROTOCOL(rtp);
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
        if (!strcmp(pc->url.head, p->name))
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
    if (0 != pc->ops->open(pc, pc->url.body, media)) {
        loge("open %s failed!\n", pc->url.body);
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

