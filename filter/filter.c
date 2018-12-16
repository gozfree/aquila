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
#include <libatomic.h>
#include <liblog.h>
#include "queue.h"
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
    REGISTER_FILTER(vencode);
    REGISTER_FILTER(vdecode);
    REGISTER_FILTER(playback);
    REGISTER_FILTER(upstream);
    REGISTER_FILTER(record);
    REGISTER_FILTER(remotectrl);
}

static void on_filter_read(int fd, void *arg)
{
    struct filter_ctx *ctx = (struct filter_ctx *)arg;
    struct aitem *in_aitem = NULL;
    struct iovec in = {NULL, 0};
    struct iovec out = {NULL, 0};
    int ret;
    int last = 0;
    if (ctx->q_src) {
        //loge("filter[%s]: before on_read in.len=%d, out.len=%d\n", ctx->name, in.iov_len, out.iov_len);
        in_aitem = aqueue_pop(ctx->q_src, fd, &last);
        if (!in_aitem) {
            loge("fd = %d, aqueue_pop empty\n", fd);
            return;
        }
        //logd("ctx = %p aqueue_pop %p, last=%d\n", ctx, in_aitem, last);
        memset(&in, 0, sizeof(struct iovec));
        memset(&out, 0, sizeof(struct iovec));
        in.iov_base = in_aitem->data.iov_base;
        in.iov_len = in_aitem->data.iov_len;
    }
    //loge("filter[%s]: before on_read in.len=%d, out.len=%d\n", ctx->name, in.iov_len, out.iov_len);
    pthread_mutex_lock(&ctx->lock);
    ret = ctx->ops->on_read(ctx, &in, &out);
    if (ret == -1) {
        //loge("filter %s on_read failed!\n", ctx->name);
    }
    //loge("filter[%s]: after on_read in.len=%d, out.len=%d\n", ctx->name, in.iov_len, out.iov_len);
    if (out.iov_base) {
        //memory create
        struct aitem *out_aitem = aitem_alloc(ctx->q_snk, out.iov_base, out.iov_len);
        //logd("fd = %d aqueue_push %p\n", fd, out_aitem);
        if (-1 == aqueue_push(ctx->q_snk, out_aitem)) {
            loge("buffer_push failed!\n");
        }
    }

    if (ctx->q_src) {
        //logd("ctx = %p aqueue_aitem_free %p, last=%d\n", ctx, in_aitem, last);
        if (last) {
            //usleep(200000);//FIXME
            //aqueue_aitem_free(in_aitem);//FIXME: double free
        }
    }
    pthread_mutex_unlock(&ctx->lock);
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

struct filter_ctx *filter_create(struct filter_conf *c,
                                 struct aqueue *q_src, struct aqueue *q_snk)
{
    int fd;
    struct filter_ctx *ctx = filter_ctx_new(c);
    if (!ctx) {
        return NULL;
    }
    loge("enter %s\n", ctx->name);
    aqueue_add_ref(q_src);
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->q_src = q_src;
    ctx->q_snk = q_snk;
    //filter types:
    //1. src filter: <NULL, snk>
    //2. mid fitler: <src, snk>
    //3. snk filter: <src, NULL>
    if (ctx->q_src) {//not sink filter
        memcpy(&ctx->media, q_src->opaque.iov_base, q_src->opaque.iov_len);
        if (ctx->q_snk) {
            q_snk->opaque.iov_base = q_src->opaque.iov_base;
            q_snk->opaque.iov_len = q_src->opaque.iov_len;
        }
    }
    logi("%s, media.video: %d*%d\n", c->type.str, ctx->media.video.width, ctx->media.video.height);
    if (-1 == ctx->ops->open(ctx)) {
        return NULL;
    }
    if (ctx->q_src) {//middle filter
        fd = aqueue_get_available_fd(q_src);
    } else {//device source filter
        fd = ctx->rfd;
        if (ctx->q_snk) {
            q_snk->opaque.iov_base = &ctx->media;
            q_snk->opaque.iov_len = sizeof(ctx->media);
        }
    }

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
    loge("leave %s\n", ctx->name);

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

static void *filter_loop(void *arg)
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
        filter_loop(ctx);
    } else {
        pthread_t tid;
        if (0 != pthread_create(&tid, NULL, filter_loop, ctx)) {
            loge("pthread_create falied: %s\n", strerror(errno));
            return -1;
        }
    }
    return 0;
}

void filter_destroy(struct filter_ctx *ctx)
{
    if (!ctx)
        return;

    loge("enter %s\n", ctx->name);
    ctx->ops->close(ctx);
    gevent_base_loop_break(ctx->ev_base);

    gevent_del(ctx->ev_base, ctx->ev_read);
    gevent_del(ctx->ev_base, ctx->ev_write);
    gevent_base_destroy(ctx->ev_base);
    free(ctx->priv);
    free(ctx);
}
