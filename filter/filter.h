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
#ifndef _FILTER_H_
#define _FILTER_H_

#include <stdint.h>
#include <stdlib.h>
#include <libmacro.h>
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
