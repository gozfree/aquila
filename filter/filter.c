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
    REGISTER_FILTER(videodec);
    REGISTER_FILTER(playback);
    REGISTER_FILTER(upstream);
    REGISTER_FILTER(record);
    REGISTER_FILTER(remotectrl);
}

static void on_src_filter_read(struct filter_ctx *ctx)
{
    struct iovec *out = NULL;
    struct queue_item *out_item = NULL;

    if (ctx->ops->alloc_data) {
        out = ctx->ops->alloc_data(ctx);
    }
    if (!out) {
        loge("filter[%s]: alloc_data NULL!\n", ctx->name);
        return;
    }
    logd("filter[%s]: %d alloc_data %p, len=%d\n", ctx->name, ctx->debug_cnt, out->iov_base, out->iov_len);
    thread_lock(ctx->thread);
    if (-1 == ctx->ops->on_read(ctx, NULL, out)) {
        loge("filter[%s] on_read failed!\n", ctx->name);
        goto exit;
    }
    logd("filter[%s]: %d on_read len=%d\n", ctx->name, ctx->debug_cnt, out->iov_len);
    out_item = queue_item_alloc(ctx->q_snk, out->iov_base, out->iov_len, ctx);
    if (!out_item) {
        loge("filter[%s] item_alloc failed!\n", ctx->name);
        goto exit;
    }
    logd("filter[%s]: %d item_alloc %p\n", ctx->name, ctx->debug_cnt, out_item);
    if (-1 == queue_push(ctx->q_snk, out_item)) {
        loge("filter[%s] buffer_push failed!\n", ctx->name);
        goto exit;
    }
    logd("filter[%s]: %d queue_push %p to   %p\n",
          ctx->name, ctx->debug_cnt, out_item, ctx->q_snk);
exit:
    free(out);
    thread_unlock(ctx->thread);
    logd("filter[%s] %d leave\n", ctx->name, ctx->debug_cnt);
}

static void on_mid_filter_read(struct filter_ctx *ctx)
{
    struct queue_item *in_item = NULL;
    struct queue_item *out_item = NULL;
    struct iovec *out = NULL;
    struct iovec *in = NULL;
    struct filter_ctx *prev_filter;

    in_item = queue_branch_pop(ctx->q_src, ctx->name);
    if (!in_item) {
        loge("filter[%s]: %d queue_branch_pop empty\n", ctx->name, ctx->debug_cnt);
        return;
    }
    prev_filter = in_item->arg;
    logd("filter[%s]: %d queue_pop  %p from %p filter[%s]\n",
          ctx->name, ctx->debug_cnt, in_item, ctx->q_src, prev_filter->name);

    in = queue_item_get_data(ctx->q_src, in_item);
    if (!in) {
        loge("filter[%s]: %d item_get_data empty\n", ctx->name, ctx->debug_cnt);
        return;
    }

    if (ctx->ops->alloc_data) {
        out = ctx->ops->alloc_data(ctx);
        logd("alloc_data %p\n", out);
    }
    if (!out) {
        loge("filter[%s]: alloc_data NULL!\n", ctx->name);
        return;
    }

    logd("filter[%s]: %d alloc_data %p, len=%d\n", ctx->name, ctx->debug_cnt, out->iov_base, out->iov_len);
    thread_lock(ctx->thread);
    if (-1 == ctx->ops->on_read(ctx, in, out)) {
        loge("filter %s on_read failed!\n", ctx->name);
        goto exit;
    }
    logd("filter[%s]: %d on_read len=%d\n", ctx->name, ctx->debug_cnt, out->iov_len);
    logd("filter[%s]: %d free_data %p by filter[%s]\n", ctx->name, ctx->debug_cnt, in, prev_filter->name);
    if (prev_filter->ops->free_data)
        prev_filter->ops->free_data(prev_filter, in);
    logd("filter[%s]: %d item_free %p by filter[%s]\n", ctx->name, ctx->debug_cnt, in_item, prev_filter->name);
    queue_item_free(ctx->q_src, in_item);
    out_item = queue_item_alloc(ctx->q_snk, out->iov_base, out->iov_len, ctx);
    if (!out_item) {
        loge("filter[%s] item_alloc failed!\n", ctx->name);
        goto exit;
    }
    logd("filter[%s]: %d item_alloc %p\n", ctx->name, ctx->debug_cnt, out_item);
    if (-1 == queue_push(ctx->q_snk, out_item)) {
        loge("filter[%s] buffer_push failed!\n", ctx->name);
        goto exit;
    }
    logd("filter[%s]: %d queue_push %p to   %p\n",
          ctx->name, ctx->debug_cnt, out_item, ctx->q_snk);

exit:
    free(out);
    thread_unlock(ctx->thread);
    logd("filter[%s] leave\n", ctx->name);
}

static void on_snk_filter_read(struct filter_ctx *ctx)
{
    struct queue_item *in_item = NULL;
    struct iovec *in = NULL;
    struct filter_ctx *prev_filter;

    in_item = queue_branch_pop(ctx->q_src, ctx->name);
    if (!in_item) {
        logi("filter[%s]: %d queue_branch_pop empty\n", ctx->name, ctx->debug_cnt);
        return;
    }
    prev_filter = in_item->arg;
    logd("filter[%s]: %d queue_pop  %p from %p filter[%s]\n",
          ctx->name, ctx->debug_cnt, in_item, ctx->q_src, prev_filter->name);

    in = queue_item_get_data(ctx->q_src, in_item);
    if (!in) {
        loge("filter[%s]: %d item_get_data empty\n", ctx->name, ctx->debug_cnt);
        return;
    }
    thread_lock(ctx->thread);
    if (-1 == ctx->ops->on_read(ctx, in, NULL)) {
        loge("filter %s on_read failed!\n", ctx->name);
        goto exit;
    }
    logd("filter[%s]: %d free_data %p by filter[%s]\n", ctx->name, ctx->debug_cnt, in, prev_filter->name);
    if (prev_filter->ops->free_data)
        prev_filter->ops->free_data(prev_filter, in);
        logd("free_data %p\n", in);
    logd("filter[%s]: %d item_free %p by filter[%s]\n", ctx->name, ctx->debug_cnt, in_item, prev_filter->name);
    queue_item_free(ctx->q_src, in_item);

exit:
    thread_unlock(ctx->thread);
    logd("filter[%s] leave\n", ctx->name);
}

static void on_filter_read(int fd, void *arg)
{
    struct filter_ctx *ctx = (struct filter_ctx *)arg;
    ctx->debug_cnt++;

    switch (ctx->type) {
    case FILTER_TYPE_SRC:
        on_src_filter_read(ctx);
        break;
    case FILTER_TYPE_MID:
        on_mid_filter_read(ctx);
        break;
    case FILTER_TYPE_SNK:
        on_snk_filter_read(ctx);
        break;
    default:
        loge("unsupported filter type!\n");
        break;
    }
}

static void on_filter_write(int fd, void *arg)
{
    struct filter_ctx *ctx = (struct filter_ctx *)arg;
    if (ctx->ops->on_write) {
        ctx->ops->on_write(ctx);
    }
}

static void *q_alloc_cb(void *data, size_t len, void *arg)
{
    return data;
}

static void q_free_cb(void *data)
{
}

struct filter_ctx *filter_create(struct filter_conf *conf,
                                 struct queue *q_src, struct queue *q_snk)
{
    int fd;
    struct queue_branch *qb;
    struct filter *f;
    struct filter_ctx *ctx = CALLOC(1, struct filter_ctx);
    if (!ctx) {
        loge("malloc filter context failed!\n");
        return NULL;
    }
    for (f = first_filter; f != NULL; f = f->next) {
        if (!strcasecmp(conf->type.str, f->name))
            break;
    }
    if (f == NULL) {
        loge("%s does not support!\n", conf->type.str);
        return NULL;
    }
    logi("\t[filter module] <%s> loaded\n", conf->type.str);

    ctx->ops = f;
    ctx->name = conf->type.str;
    ctx->url = conf->url;
    ctx->conf = conf;

    queue_branch_new(q_src, ctx->name);
    ctx->q_src = q_src;
    ctx->q_snk = q_snk;

    if (!q_src && q_snk) {
        ctx->type = FILTER_TYPE_SRC;
    } else if (q_src && q_snk) {
        ctx->type = FILTER_TYPE_MID;
    } else if (q_src && !q_snk) {
        ctx->type = FILTER_TYPE_SNK;
    }

    /*
     * prepare: transfer configure to next filters
     */
    switch (ctx->type) {
    case FILTER_TYPE_SRC:
        q_snk->opaque.iov_base = ctx->conf;
        q_snk->opaque.iov_len  = sizeof(*ctx->conf);
        break;
    case FILTER_TYPE_MID:
        ctx->prev_conf = q_src->opaque.iov_base;
        q_snk->opaque.iov_base = ctx->conf;
        q_snk->opaque.iov_len  = sizeof(*ctx->conf);
        break;
    case FILTER_TYPE_SNK:
        ctx->prev_conf = q_src->opaque.iov_base;
        break;
    default:
        break;
    }

#if 0
    //filter types:
    //1. src filter: <NULL, snk>
    //2. mid fitler: <src, snk>
    //3. snk filter: <src, NULL>
    if (ctx->q_src) {//mid/snk filter
        loge("%s, media.video: %d*%d, format=%d, extra.len=%d\n",
             conf->type.str, ctx->media_encoder.video.width, ctx->media_encoder.video.height, ctx->media_encoder.video.format, ctx->media_encoder.video.extra_size);
        media_encoder_dump_info(&ctx->media_encoder);
        memcpy(&ctx->media_encoder, q_src->opaque.iov_base, q_src->opaque.iov_len);
        media_encoder_dump_info(&ctx->media_encoder);
        loge("%s, media.video: %d*%d, format=%d, extra.len=%d\n", conf->type.str, ctx->media_encoder.video.width, ctx->media_encoder.video.height, ctx->media_encoder.video.format, ctx->media_encoder.video.extra_size);
        if (ctx->q_snk) {
            q_snk->opaque.iov_base = q_src->opaque.iov_base;
            q_snk->opaque.iov_len = q_src->opaque.iov_len;
        }
    }
#endif
    if (-1 == ctx->ops->open(ctx)) {
        return NULL;
    }

    /*
     * after
     */
    switch (ctx->type) {
    case FILTER_TYPE_SRC:
        fd = ctx->rfd;
        break;
    case FILTER_TYPE_MID:
        qb = queue_branch_get(q_src, ctx->name);
        fd = qb->RD_FD;
        break;
    case FILTER_TYPE_SNK:
        qb = queue_branch_get(q_src, ctx->name);
        fd = qb->RD_FD;
        break;
    default:
        break;
    }

#if 0
    if (ctx->q_src) {//middle filter
        qb = queue_branch_get(q_src, ctx->name);
        fd = qb->RD_FD;
        if (ctx->q_snk) {
        q_snk->opaque.iov_base = &ctx->media_encoder;
        q_snk->opaque.iov_len = sizeof(ctx->media_encoder);
        }
    } else {//device source filter
        fd = ctx->rfd;
        if (ctx->q_snk) {
            q_snk->opaque.iov_base = &ctx->media_encoder;
            q_snk->opaque.iov_len = sizeof(ctx->media_encoder);
        }
    }
#endif
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
    if (-1 == gevent_add(ctx->ev_base, &ctx->ev_read)) {
        goto failed;
    }

    if (0) {
    ctx->ev_write = gevent_create(ctx->wfd, NULL, on_filter_write, NULL, ctx);
    if (!ctx->ev_write) {
        goto failed;
    }
    if (-1 == gevent_add(ctx->ev_base, &ctx->ev_write)) {
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
        gevent_del(ctx->ev_base, &ctx->ev_read);
    }
    if (ctx->ev_write) {
        gevent_del(ctx->ev_base, &ctx->ev_write);
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

int filter_stop(struct filter_ctx *ctx)
{
    if (!ctx)
        return -1;

    gevent_base_loop_break(ctx->ev_base);
    thread_join(ctx->thread);

    gevent_del(ctx->ev_base, &ctx->ev_read);
    if (0) {
    gevent_del(ctx->ev_base, &ctx->ev_write);
    }

    return 0;
}

void filter_destroy(struct filter_ctx *ctx)
{
    if (!ctx)
        return;

    logi("filter[%s]: destroy\n", ctx->name);
    ctx->ops->close(ctx);

    queue_branch_del(ctx->q_src, ctx->name);
    queue_branch_del(ctx->q_snk, ctx->name);
    logi("filter[%s]: queue_flush q_src=%p, q_snk=%p\n", ctx->name, ctx->q_src, ctx->q_snk);
    queue_flush(ctx->q_src);
    queue_flush(ctx->q_snk);
    gevent_destroy(ctx->ev_read);
    gevent_base_destroy(ctx->ev_base);
    thread_destroy(ctx->thread);
    ctx->thread = NULL;
    free(ctx);
}
