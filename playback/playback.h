/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    playback.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-02 19:07
 * updated: 2016-05-02 19:07
 ******************************************************************************/
#ifndef _PLAYBACK_H_
#define _PLAYBACK_H_

#include <stdint.h>
#include <stdlib.h>
#include "common.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

struct playback_ctx {
    struct url url;
    struct playback *ops;
    struct media_params media;
    void *priv;
};

struct playback {
    const char *name;
    int (*open)(struct playback_ctx *c, const char *type, struct media_params *format);
    int (*read)(struct playback_ctx *c, void *buf, int len);
    int (*write)(struct playback_ctx *c, void *buf, int len);
    void (*close)(struct playback_ctx *c);
    int priv_size;
    struct playback *next;
};

struct playback_ctx *playback_open(const char *url, struct media_params *format);
int playback_read(struct playback_ctx *c, void *buf, int len);
int playback_write(struct playback_ctx *c, void *buf, int len);
void playback_close(struct playback_ctx *c);

void playback_register_all();

#ifdef __cplusplus
}
#endif
#endif
