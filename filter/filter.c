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
#include <gear-lib/libqueue.h>
#include "filter.h"


#define REGISTER_FILTER(x)                                                     \
    {                                                                          \
        extern struct filter aq_##x##_filter;                                  \
            filter_register(&aq_##x##_filter);                                 \
    }

static struct filter *first_filter = NULL;
static struct filter **last_filter = &first_filter;
static int registered = 0;

static void filter_register(struct filter *f)
{
    struct filter **p = last_filter;
    f->next = NULL;
    while (*p || atomic_ptr_cas((void * volatile *)p, NULL, f))
        p = &(*p)->next;
    last_filter = &f->next;
}

void filter_register_all(void)
{
    if (registered)
        return;
    registered = 1;

    REGISTER_FILTER(videocap);
    REGISTER_FILTER(videoenc);
    REGISTER_FILTER(vdecode);
    REGISTER_FILTER(playback);
    REGISTER_FILTER(upstream);
    REGISTER_FILTER(record);
    REGISTER_FILTER(remotectrl);
}

static void on_filter_read(int fd, void *arg)
{
    struct filter_ctx *ctx = (struct filter_ctx *)arg;
    struct item *in_item = NULL;
    struct iovec in = {NULL, 0};
    struct iovec out = {NULL, 0};
    int ret;
    int last = 0;
    logd("filtre[%s] enter\n", ctx->name);
    if (ctx->q_src) {
        in_item = queue_branch_pop(ctx->q_src, ctx->name);
        if (!in_item) {
            loge("fd = %d, aqueue_pop empty\n", fd);
            return;
        }
        //logd("ctx = %p aqueue_pop %p, last=%d\n", ctx, in_aitem, last);
        memset(&in, 0, sizeof(struct iovec));
        memset(&out, 0, sizeof(struct iovec));
        struct iovec *tmp_io = item_get_data(ctx->q_src, in_item);
        in.iov_base = tmp_io->iov_base;
        in.iov_len = tmp_io->iov_len;
        logd("filter[%s]: queue_pop %p\n", ctx->name, in.iov_base);
    }
    //loge("filter[%s]: before on_read in.len=%d, out.len=%d\n", ctx->name, in.iov_len, out.iov_len);
    thread_lock(ctx->thread);
    ret = ctx->ops->on_read(ctx, &in, &out);
    if (ret == -1) {
        //loge("filter %s on_read failed!\n", ctx->name);
    }
    logd("filter[%s]: after on_read in.len=%d, out.len=%d\n", ctx->name, in.iov_len, out.iov_len);
    if (out.iov_base) {
        //memory create
        struct item *out_item = item_alloc(ctx->q_snk, out.iov_base, out.iov_len, NULL);
        if (-1 == queue_push(ctx->q_snk, out_item)) {
            loge("buffer_push failed!\n");
        }
        logd("filter[%s]: queue_push %p\n", ctx->name, out_item->opaque.iov_base);
    }

    if (ctx->q_src) {
        //logd("ctx = %p aqueue_item_free %p, last=%d\n", ctx, in_item, last);
        if (last) {
            //usleep(200000);//FIXME
            //aqueue_item_free(in_item);//FIXME: double free
        }
    }
    thread_unlock(ctx->thread);
    logd("filtre[%s] leave\n", ctx->name);
}

static void on_filter_write(int fd, void *arg)
{
    struct filter_ctx *ctx = (struct filter_ctx *)arg;
    if (ctx->ops->on_write) {
        ctx->ops->on_write(ctx);
    }
}

struct filter_ctx *filter_ctx_new(struct filter_conf *c)
{
    struct filter *p;
    struct filter_ctx *fc = CALLOC(1, struct filter_ctx);
    if (!fc) {
        loge("malloc filter context failed!\n");
        return NULL;
    }
    for (p = first_filter; p != NULL; p = p->next) {
        if (!strcasecmp(c->type.str, p->name))
            break;
    }
    if (p == NULL) {
        loge("%s protocol is not support!\n", c->type.str);
        return NULL;
    }
    logd("\t[filter module] <%s> loaded\n", c->type.str);

    fc->ops = p;
    fc->name = c->type.str;
    fc->url = c->url;
    fc->config = c;
    return fc;
}

static void *q_alloc_cb(void *data, size_t len, void *arg)
{
    return data;
}

static void q_free_cb(void *data)
{
}

struct filter_ctx *filter_create(struct filter_conf *c,
                                 struct queue *q_src, struct queue *q_snk)
{
    int fd;
    struct queue_branch *qb;
    struct filter_ctx *ctx = filter_ctx_new(c);
    if (!ctx) {
        return NULL;
    }
    queue_branch_new(q_src, ctx->name);
    ctx->q_src = q_src;
    ctx->q_snk = q_snk;
    //filter types:
    //1. src filter: <NULL, snk>
    //2. mid fitler: <src, snk>
    //3. snk filter: <src, NULL>
    if (ctx->q_src) {//mid/snk filter
        logd("%s, media.video: %d*%d, extra.len=%d\n", c->type.str, ctx->media_encoder.video.width, ctx->media_encoder.video.height, ctx->media_encoder.video.extra_size);
        memcpy(&ctx->media_encoder, q_src->opaque.iov_base, q_src->opaque.iov_len);
        logd("%s, media.video: %d*%d, extra.len=%d\n", c->type.str, ctx->media_encoder.video.width, ctx->media_encoder.video.height, ctx->media_encoder.video.extra_size);
        if (ctx->q_snk) {
            q_snk->opaque.iov_base = q_src->opaque.iov_base;
            q_snk->opaque.iov_len = q_src->opaque.iov_len;
            ctx->type = MID_FILTER;
        }
    }
    if (-1 == ctx->ops->open(ctx)) {
        return NULL;
    }
    if (ctx->q_src) {//middle filter
        qb = queue_branch_get(q_src, ctx->name);
        fd = qb->RD_FD;
        if (ctx->q_snk) {
        q_snk->opaque.iov_base = &ctx->media_encoder;
        q_snk->opaque.iov_len = sizeof(ctx->media_encoder);
        }
    } else {//device source filter
        ctx->type = SRC_FILTER;
        fd = ctx->rfd;
        if (ctx->q_snk) {
            q_snk->opaque.iov_base = &ctx->media_encoder;
            q_snk->opaque.iov_len = sizeof(ctx->media_encoder);
        }
    }
    queue_set_hook(ctx->q_src, q_alloc_cb, q_free_cb);
    queue_set_hook(ctx->q_snk, q_alloc_cb, q_free_cb);

    ctx->ev_base = gevent_base_create();
    if (!ctx->ev_base) {
        goto failed;
    }

    ctx->ev_read = gevent_create(fd, on_filter_read, NULL, NULL, ctx);
    if (!ctx->ev_read) {
        goto failed;
    }
    if (-1 == gevent_add(ctx->ev_base, ctx->ev_read)) {
        goto failed;
    }

    if (0) {
    ctx->ev_write = gevent_create(ctx->wfd, NULL, on_filter_write, NULL, ctx);
    if (!ctx->ev_write) {
        goto failed;
    }
    if (-1 == gevent_add(ctx->ev_base, ctx->ev_write)) {
        goto failed;
    }
    }
    logd("leave %s\n", ctx->name);

    return ctx;

failed:
    if (!ctx) {
        return NULL;
    }
    if (ctx->ev_read) {
        gevent_del(ctx->ev_base, ctx->ev_read);
    }
    if (ctx->ev_write) {
        gevent_del(ctx->ev_base, ctx->ev_write);
    }
    if (ctx->ev_base) {
        gevent_base_destroy(ctx->ev_base);
    }
    free(ctx);
    return NULL;
}

static void *filter_loop(struct thread *t, void *arg)
{
    struct filter_ctx *ctx = (struct filter_ctx *)arg;
    gevent_base_loop(ctx->ev_base);
    return NULL;
}

int filter_dispatch(struct filter_ctx *ctx, int block)
{
    if (!ctx) {
        return -1;
    }
    if (block) {
        filter_loop(NULL, ctx);
    } else {
        ctx->thread = thread_create(filter_loop, ctx);
        if (!ctx->thread) {
            loge("thread_create falied: %s\n", strerror(errno));
            return -1;
        }
        thread_set_name(ctx->thread, ctx->name);
    }
    return 0;
}

void filter_destroy(struct filter_ctx *ctx)
{
    if (!ctx)
        return;

    ctx->ops->close(ctx);
    gevent_base_loop_break(ctx->ev_base);

    gevent_del(ctx->ev_base, ctx->ev_read);
    gevent_del(ctx->ev_base, ctx->ev_write);
    gevent_base_destroy(ctx->ev_base);

    queue_branch_del(ctx->q_src, ctx->name);
    queue_branch_del(ctx->q_snk, ctx->name);
    free(ctx);
}
