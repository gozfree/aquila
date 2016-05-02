/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    filter.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-04-30 16:02
 * updated: 2016-04-30 16:02
 ******************************************************************************/
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
    REGISTER_FILTER(playback);
}

static void on_filter_read(int fd, void *arg)
{
    struct filter_ctx *ctx = (struct filter_ctx *)arg;
    struct queue_item *in_item = NULL;
    void *in_data = NULL;
    void *out_data = NULL;
    int in_len = 0;
    int out_len = 0;
    int last = 0;
    if (ctx->q_src) {
        in_item = queue_pop(ctx->q_src, fd, &last);
        if (!in_item) {
            loge("fd = %d, queue_pop empty\n", fd);
            return;
        }
        //logd("ctx = %p queue_pop %p, last=%d\n", ctx, in_item, last);
        in_data = in_item->data;
        in_len = in_item->len;
    }
    pthread_mutex_lock(&ctx->lock);
    ctx->ops->on_read(ctx->priv, in_data, in_len, &out_data, &out_len);
    if (out_data) {
        //memory create
        struct queue_item *out_item = queue_item_new(out_data, out_len);
        //logd("fd = %d queue_push %p\n", fd, out_item);
        if (-1 == queue_push(ctx->q_snk, out_item)) {
            loge("buffer_push failed!\n");
        }
    }

    if (ctx->q_src) {
        //logd("ctx = %p queue_item_free %p, last=%d\n", ctx, in_item, last);
        if (last) {
            usleep(100000);//FIXME
            queue_item_free(in_item);
        }
    }
    pthread_mutex_unlock(&ctx->lock);
}

static void on_filter_write(int fd, void *arg)
{
    struct filter_ctx *ctx = (struct filter_ctx *)arg;
    if (ctx->ops->on_write) {
        ctx->ops->on_write(ctx->priv);
    }
}

struct filter_ctx *filter_ctx_new(const char *name)
{
    struct filter *p;
    struct filter_ctx *fc = CALLOC(1, struct filter_ctx);
    if (!fc) {
        loge("malloc filter context failed!\n");
        return NULL;
    }
    for (p = first_filter; p != NULL; p = p->next) {
        if (!strcmp(name, p->name))
            break;
    }
    if (p == NULL) {
        loge("%s protocol is not support!\n", name);
        return NULL;
    }
    logi("\t[filter module] <%s> loaded\n", name);

    fc->ops = p;
    return fc;
}

struct filter_ctx *filter_create(const char *name,
                                 struct queue *q_src, struct queue *q_snk)
{
    int fd;
    struct filter_ctx *ctx = filter_ctx_new(name);
    if (!ctx) {
        return NULL;
    }
    queue_add_ref(q_src);
    pthread_mutex_init(&ctx->lock, NULL);
    ctx->q_src = q_src;
    ctx->q_snk = q_snk;
    if (ctx->q_src) {//middle filter
        memcpy(&ctx->media, &q_src->media, sizeof(q_src->media));
        logd("media.video: %d*%d\n",
             ctx->media.video.width, ctx->media.video.height);
    }
    if (-1 == ctx->ops->open(ctx)) {
        return NULL;
    }
    if (ctx->q_src) {//middle filter
        fd = queue_get_available_fd(q_src);
    } else {//device source filter
        fd = ctx->rfd;
        if (ctx->q_snk) {
            memcpy(&q_snk->media, &ctx->media, sizeof(ctx->media));
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

    gevent_base_loop_break(ctx->ev_base);

    gevent_del(ctx->ev_base, ctx->ev_read);
    gevent_del(ctx->ev_base, ctx->ev_write);
    gevent_base_destroy(ctx->ev_base);
    free(ctx->priv);
    free(ctx);
}
