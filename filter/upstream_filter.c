/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    upstream_filter.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-22 22:59
 * updated: 2016-05-22 22:59
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <liblog.h>

#include "queue.h"
#include "protocol.h"
#include "filter.h"
#include "common.h"

struct upstream_ctx {
    struct protocol_ctx *proto;
    struct upstream_conf *conf;
};

static int on_upstream_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct upstream_ctx *ctx = (struct upstream_ctx *)fc->priv;
    struct protocol_ctx *pc = ctx->proto;
    int ret = protocol_write(pc, in->iov_base, in->iov_len);
    if (ret == -1) {
        loge("protocol_write failed!\n");
    }
    return ret;
}

static int upstream_open(struct filter_ctx *fc)
{
    struct protocol_ctx *proto = protocol_open(fc->url, &fc->media);
    if (!proto) {
        loge("open protocol %s failed!\n", fc->url);
        return -1;
    }
    struct upstream_ctx *uc = CALLOC(1, struct upstream_ctx);
    if (!uc) {
        loge("malloc upstream_ctx failed!\n");
        goto failed;
    }
    uc->proto = proto;
    fc->rfd = -1;
    fc->wfd = -1;
    fc->priv = uc;
    return 0;
failed:
    if (proto) {
        protocol_close(proto);
    }
    if (uc) {
        free(uc);
    }
    return -1;
}

static void upstream_close(struct filter_ctx *fc)
{
    struct upstream_ctx *c = (struct upstream_ctx *)fc->priv;
    if (c->proto) {
        protocol_close(c->proto);
        free(c->proto);
    }
}

struct filter aq_upstream_filter = {
    .name     = "upstream",
    .open     = upstream_open,
    .on_read  = on_upstream_read,
    .on_write = NULL,
    .close    = upstream_close,
};
