/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    protocol.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-22 22:31
 * updated: 2016-05-22 22:31
 ******************************************************************************/
#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>
#include <stdlib.h>
#include "common.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

struct protocol_ctx {
    struct url url;
    struct protocol *ops;
    void *priv;
};

struct protocol {
    const char *name;
    int (*open)(struct protocol_ctx *c, const char *url, struct media_params *media);
    int (*read)(struct protocol_ctx *c, void *buf, int len);
    int (*write)(struct protocol_ctx *c, void *buf, int len);
    void (*close)(struct protocol_ctx *c);
    struct protocol *next;
};

void protocol_register_all();
struct protocol_ctx *protocol_new(const char *url);
void protocol_free(struct protocol_ctx *c);

struct protocol_ctx *protocol_open(const char *url, struct media_params *media);

int protocol_read(struct protocol_ctx *c, void *buf, int len);
int protocol_write(struct protocol_ctx *c, void *buf, int len);
void protocol_close(struct protocol_ctx *c);


#ifdef __cplusplus
}
#endif
#endif
