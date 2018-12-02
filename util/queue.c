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
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <libmacro.h>
#include <liblog.h>
#include <libatomic.h>
#include "queue.h"

#define QUEUE_MAX_DEPTH 200

struct aitem *aitem_alloc(struct aqueue *q, void *data, size_t len)
{
    if (!q) {
        return NULL;
    }
    struct aitem *aitem = CALLOC(1, struct aitem);
    if (!aitem) {
        printf("malloc failed!\n");
        return NULL;
    }
    if (q->alloc_hook) {
        aitem->opaque.iov_base = (q->alloc_hook)(data, len);
        aitem->opaque.iov_len = len;
    } else {
        aitem->data.iov_base = memdup(data, len);
        aitem->data.iov_len = len;
    }
    aitem->ref_cnt = 0;
    return aitem;
}

void aitem_free(struct aqueue *q, struct aitem *aitem)
{
    if (!q) {
        return;
    }
    if (!aitem) {
        return;
    }

    int ref_cnt = atomic_int_dec(&aitem->ref_cnt);
    if (ref_cnt > 0) {
        logw("aqueue aitem is used, ref_cnt = %d", ref_cnt);
        return;
    }
    if (q->free_hook) {
        (q->free_hook)(aitem->opaque.iov_base);
        aitem->opaque.iov_len = 0;
    } else {
        free(aitem->data.iov_base);
    }
    free(aitem);
}

int aqueue_set_mode(struct aqueue *q, enum aqueue_mode mode)
{
    if (!q) {
        return -1;
    }
    q->mode = mode;
    return 0;
}

int aqueue_set_hook(struct aqueue *q, alloc_hook *alloc_cb, free_hook *free_cb)
{
    if (!q) {
        return -1;
    }
    q->alloc_hook = alloc_cb;
    q->free_hook = free_cb;
    return 0;
}

int aqueue_set_depth(struct aqueue *q, int depth)
{
    if (!q) {
        return -1;
    }
    q->max_depth = depth;
    return 0;
}

struct aqueue *aqueue_create()
{
    struct aqueue *q = CALLOC(1, struct aqueue);
    if (!q) {
        loge("malloc failed!\n");
        return NULL;
    }
    INIT_LIST_HEAD(&q->head);
    INIT_LIST_HEAD(&q->pipe_list);
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->ref_cnt = 0;
    q->depth = 0;
    q->max_depth = QUEUE_MAX_DEPTH;
    q->mode = QUEUE_FULL_FLUSH;
    q->alloc_hook = NULL;
    q->free_hook = NULL;
    return q;
}

int aqueue_flush(struct aqueue *q)
{
    if (!q) {
        return -1;
    }
    struct aitem *aitem, *next;
    pthread_mutex_lock(&q->lock);
    list_for_each_entry_safe(aitem, next, &q->head, entry) {
        list_del(&aitem->entry);
        aitem_free(q, aitem);
    }
    q->depth = 0;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

void aqueue_destroy(struct aqueue *q)
{
    struct pipe_fds *pipe_aitem, *pipe_next;
    if (!q) {
        return;
    }
    aqueue_flush(q);
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
    list_for_each_entry_safe(pipe_aitem, pipe_next, &q->pipe_list, entry) {
        close(pipe_aitem->rfd);
        close(pipe_aitem->wfd);
    }

    free(q);
}


int aqueue_get_available_fd(struct aqueue *q)
{
    if (!q) {
        return -1;
    }
    struct pipe_fds *pipe_aitem, *next;
    list_for_each_entry_safe(pipe_aitem, next, &q->pipe_list, entry) {
        if (!pipe_aitem->used) {
            pipe_aitem->used = 1;
            return pipe_aitem->rfd;
        }
    }
    return -1;
}

int aqueue_add_ref(struct aqueue *q)
{
    if (!q) {
        return -1;
    }
    int fds[2];
    if (pipe(fds)) {
        printf("create pipe failed: %s\n", strerror(errno));
        return -1;
    }
    struct pipe_fds *pfds = CALLOC(1, struct pipe_fds);
    pfds->rfd = fds[0];
    pfds->wfd = fds[1];
    logd("pipe: rfd = %d, wfd = %d\n", pfds->rfd, pfds->wfd);
    pfds->used = 0;
    list_add_tail(&pfds->entry, &q->pipe_list);
    q->ref_cnt++;
    return 0;
}

#if 0
struct aitem *aqueue_pop_absolute(struct aqueue *q)
{
    if (!q) {
        return NULL;
    }
    if (list_empty(&q->head)) {
        return NULL;
    }
    char notify;
    struct aitem *aitem = NULL;
    struct pipe_fds *pipe_aitem, *next;
    pthread_mutex_lock(&q->lock);
    aitem = list_first_entry_or_null(&q->head, struct aitem, entry);
    if (aitem) {
        list_del(&aitem->entry);
        --(q->depth);
        list_for_each_entry_safe(pipe_aitem, next, &q->pipe_list, entry) {
            if (read(pipe_aitem->rfd, &notify, sizeof(notify)) != 1) {
                printf("read pipe fd = %d failed: %s\n", pipe_aitem->rfd, strerror(errno));
            }
        }
    }
    pthread_mutex_unlock(&q->lock);
    return aitem;
}
#endif

int aqueue_push(struct aqueue *q, struct aitem *aitem)
{
    if (!q || !aitem) {
        loge("invalid paraments!\n");
        return -1;
    }
    char notify = '1';
    struct pipe_fds *pipe_aitem, *next;
    pthread_mutex_lock(&q->lock);
    atomic_int_set(&aitem->ref_cnt, q->ref_cnt);
    //printf("push aitem= %p, ref= %d\n", aitem, atomic_get(&aitem->ref));
    list_add_tail(&aitem->entry, &q->head);
    ++(q->depth);

    list_for_each_entry_safe(pipe_aitem, next, &q->pipe_list, entry) {
        if (write(pipe_aitem->wfd, &notify, sizeof(notify)) != 1) {
            printf("write pipe failed: %s\n", strerror(errno));
        }
    }
    pthread_mutex_unlock(&q->lock);
    if (q->depth > q->max_depth) {
        //printf("aqueue depth reach max depth %d\n", q->depth);
        //aqueue_pop_absolute(q);
    }
    return 0;
}

struct aitem *aqueue_pop(struct aqueue *q, int fd, int *last)
{
    if (!q) {
        printf("invalid parament!\n");
        return NULL;
    }
    char notify;
#if 1
    if (read(fd, &notify, sizeof(notify)) != 1) {
        printf("read pipe fd = %d failed: %s\n", fd, strerror(errno));
    }
#endif
    if (list_empty(&q->head)) {
        return NULL;
    }
    struct aitem *aitem = NULL;
    pthread_mutex_lock(&q->lock);
    aitem = list_first_entry_or_null(&q->head, struct aitem, entry);
    if (aitem) {
        atomic_int_dec(&aitem->ref_cnt);
        if (!atomic_int_get(&aitem->ref_cnt)) {
            *last = 1;
            list_del(&aitem->entry);
            --(q->depth);
#if 0
            if (read(fd, &notify, sizeof(notify)) != 1) {
                printf("read pipe fd = %d failed: %s\n", fd, strerror(errno));
            }
#endif
        }
        //printf("pop aitem= %p, ref= %d\n", aitem, atomic_get(&aitem->ref));
    }
    pthread_mutex_unlock(&q->lock);
    return aitem;
}

int aqueue_get_depth(struct aqueue *q)
{
    return q->depth;
}
