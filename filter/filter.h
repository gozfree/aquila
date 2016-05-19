/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    filter.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-04-30 16:03
 * updated: 2016-04-30 16:03
 ******************************************************************************/
#ifndef _FILTER_H_
#define _FILTER_H_

#include <stdint.h>
#include <stdlib.h>
#include <libgzf.h>
#include <libgevent.h>
#include "common.h"
#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

struct filter_ctx {
    int rfd;
    int wfd;
    const char *name;
    pthread_mutex_t lock;
    struct gevent_base *ev_base;
    struct gevent *ev_read;
    struct gevent *ev_write;
    struct queue *q_src;
    struct queue *q_snk;
    struct filter *ops;
    struct media_params media;
    const char *url;
    void *priv;
    void *config;
};


struct filter {
    const char *name;
    int (*open)(struct filter_ctx *c);
    int (*on_read)(struct filter_ctx *c, struct iovec *in, struct iovec *out);
    int (*on_write)(struct filter_ctx *c);
    void (*close)(struct filter_ctx *c);
    struct filter *next;
};

void filter_register_all(void);

struct filter_ctx *filter_create(struct filter_conf *c,
                                 struct queue *q_src, struct queue *q_snk);
int filter_dispatch(struct filter_ctx *ctx, int block);
void filter_destroy(struct filter_ctx *ctx);

#ifdef __cplusplus
}
#endif
#endif
