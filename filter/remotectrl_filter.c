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
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <liblog.h>

#include "queue.h"
#include "protocol.h"
#include "filter.h"
#include "common.h"

struct remotectrl_ctx {
    struct protocol_ctx *proto;
    struct remotectrl_conf *conf;
};

static int on_remotectrl_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct remotectrl_ctx *ctx = (struct remotectrl_ctx *)fc->priv;
    struct protocol_ctx *pc = ctx->proto;
    int ret = protocol_write(pc, in->iov_base, in->iov_len);
    if (ret == -1) {
        loge("protocol_write failed!\n");
    }
    return ret;
}

static int remotectrl_open(struct filter_ctx *fc)
{
    struct protocol_ctx *proto = protocol_open(fc->url, &fc->media);
    if (!proto) {
        loge("open protocol %s failed!\n", fc->url);
        return -1;
    }
    struct remotectrl_ctx *uc = CALLOC(1, struct remotectrl_ctx);
    if (!uc) {
        loge("malloc remotectrl_ctx failed!\n");
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

static void remotectrl_close(struct filter_ctx *fc)
{
    struct remotectrl_ctx *c = (struct remotectrl_ctx *)fc->priv;
    if (c->proto) {
        protocol_close(c->proto);
        free(c->proto);
    }
}

struct filter aq_remotectrl_filter = {
    .name     = "remotectrl",
    .open     = remotectrl_open,
    .on_read  = on_remotectrl_read,
    .on_write = NULL,
    .close    = remotectrl_close,
};
