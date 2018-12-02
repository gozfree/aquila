/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    playback.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-02 19:08
 * updated: 2016-05-02 19:08
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libmacro.h>
#include <libatomic.h>
#include <liblog.h>

#include "playback.h"

#define REGISTER_PLAYBACK(x)                                                   \
    {                                                                          \
        extern struct playback aq_##x##_playback;                              \
            playback_register(&aq_##x##_playback);                             \
    }


static struct playback *first_playback = NULL;
static struct playback **last_playback = &first_playback;
static int registered = 0;

static void playback_register(struct playback *ply)
{
    struct playback **p = last_playback;
    ply->next = NULL;
    while (*p || atomic_ptr_cas((void * volatile *)p, NULL, ply))
        p = &(*p)->next;
    last_playback = &ply->next;
}

void playback_register_all()
{
    if (registered)
        return;
    registered = 1;

    REGISTER_PLAYBACK(sdl);
    REGISTER_PLAYBACK(snk);
}

struct playback_ctx *playback_open(const char *url, struct media_params *format)
{
    struct playback *p;
    struct playback_ctx *pc = CALLOC(1, struct playback_ctx);
    if (!pc) {
        loge("malloc playback context failed!\n");
        return NULL;
    }
    if (-1 == url_parse(&pc->url, url)) {
        loge("url_parse failed!\n");
        return NULL;
    }

    for (p = first_playback; p != NULL; p = p->next) {
        if (!strcmp(pc->url.head, p->name))
            break;
    }
    if (p == NULL) {
        loge("%s playback is not support!\n", pc->url.head);
        goto failed;
    }
    logi("[playback module] <%s> loaded\n", p->name);

    pc->ops = p;
    if (!pc->ops->open) {
        loge("playback open ops can't be null\n");
        goto failed;
    }
    if (0 != pc->ops->open(pc, pc->url.body, format)) {
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

int playback_read(struct playback_ctx *c, void *buf, int len)
{
    if (!c->ops->read)
        return -1;
    return c->ops->read(c, buf, len);
}

int playback_write(struct playback_ctx *c, void *buf, int len)
{
    if (!c->ops->write)
        return -1;
    return c->ops->write(c, buf, len);
}

void playback_close(struct playback_ctx *c)
{
    if (!c) {
        return;
    }
    if (c->ops->close) {
        c->ops->close(c);
    }
    free(c);
}
