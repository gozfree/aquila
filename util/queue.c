/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    queue.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-01 15:16
 * updated: 2016-05-01 15:16
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <libgzf.h>
#include <liblog.h>
#include <libatomic.h>
#include "queue.h"

#define QUEUE_MAX_DEPTH 10

struct queue_item *queue_item_new(void *data, int len)
{
    struct queue_item *item = CALLOC(1, struct queue_item);
    if (!item) {
        loge("malloc failed!\n");
        return NULL;
    }
    item->data = data;
    item->len = len;
    item->ref_cnt = 0;
    return item;
}

void queue_item_free(struct queue_item *item)
{
    if (!item) {
        return;
    }
    int ref_cnt = atomic_int_dec(&item->ref_cnt);
    if (ref_cnt > 0) {
        logw("queue item is used, ref_cnt = %d", ref_cnt);
        return;
    }
    if (item->data) {
        free(item->data);
    }
    free(item);
    item = NULL;
}

int queue_get_available_fd(struct queue *q)
{
    struct pipe_fds *pipe_item, *next;
    list_for_each_entry_safe(pipe_item, next, &q->pipe_list, entry) {
        if (!pipe_item->used) {
            pipe_item->used = 1;
            return pipe_item->rfd;
        }
    }
    return -1;
}

#if 1
int queue_add_ref(struct queue *q)
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
#endif

#if 0
struct queue_item *queue_pop_absolute(struct queue *q)
{
    if (!q) {
        return NULL;
    }
    if (list_empty(&q->head)) {
        return NULL;
    }
    char notify;
    struct queue_item *item = NULL;
    struct pipe_fds *pipe_item, *next;
    pthread_mutex_lock(&q->lock);
    item = list_first_entry_or_null(&q->head, struct queue_item, entry);
    if (item) {
        list_del(&item->entry);
        --(q->depth);
        list_for_each_entry_safe(pipe_item, next, &q->pipe_list, entry) {
            if (read(pipe_item->rfd, &notify, sizeof(notify)) != 1) {
                printf("read pipe fd = %d failed: %s\n", pipe_item->rfd, strerror(errno));
            }
        }
    }
    pthread_mutex_unlock(&q->lock);
    return item;
}
#endif

struct queue_item *queue_pop(struct queue *q, int fd, int *last)
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
    struct queue_item *item = NULL;
    pthread_mutex_lock(&q->lock);
    item = list_first_entry_or_null(&q->head, struct queue_item, entry);
    if (item) {
        atomic_int_dec(&item->ref_cnt);
        if (!atomic_int_get(&item->ref_cnt)) {
            *last = 1;
            list_del(&item->entry);
            --(q->depth);
#if 0
            if (read(fd, &notify, sizeof(notify)) != 1) {
                printf("read pipe fd = %d failed: %s\n", fd, strerror(errno));
            }
#endif
        }
        //printf("pop item= %p, ref= %d\n", item, atomic_get(&item->ref));
    }
    pthread_mutex_unlock(&q->lock);
    return item;
}

int queue_push(struct queue *q, struct queue_item *item)
{
    if (!q || !item) {
        loge("invalid paraments!\n");
        return -1;
    }
    char notify = '1';
    struct pipe_fds *pipe_item, *next;
    pthread_mutex_lock(&q->lock);
    atomic_int_set(&item->ref_cnt, q->ref_cnt);
    //printf("push item= %p, ref= %d\n", item, atomic_get(&item->ref));
    list_add_tail(&item->entry, &q->head);
    ++(q->depth);

    list_for_each_entry_safe(pipe_item, next, &q->pipe_list, entry) {
        if (write(pipe_item->wfd, &notify, sizeof(notify)) != 1) {
            printf("write pipe failed: %s\n", strerror(errno));
        }
    }
    pthread_mutex_unlock(&q->lock);
    if (q->depth > q->max_depth) {
        //printf("queue depth reach max depth %d\n", q->depth);
        //queue_pop_absolute(q);
    }
    return 0;
}

struct queue *queue_create()
{
    struct queue *q = CALLOC(1, struct queue);
    if (!q) {
        loge("malloc failed!\n");
        return NULL;
    }
    INIT_LIST_HEAD(&q->head);
    INIT_LIST_HEAD(&q->pipe_list);
    pthread_mutex_init(&q->lock, NULL);

    q->ref_cnt = 0;
    q->depth = 0;
    q->max_depth = QUEUE_MAX_DEPTH;
    //queue_add_ref(q);

    return q;
}

void queue_free(struct queue *q)
{
    struct queue_item *item, *next;
    struct pipe_fds *pipe_item, *pipe_next;
    if (!q) {
        return;
    }

    list_for_each_entry_safe(item, next, &q->head, entry) {
        list_del(&item->entry);
        queue_item_free(item);
    }
    pthread_mutex_destroy(&q->lock);
    list_for_each_entry_safe(pipe_item, pipe_next, &q->pipe_list, entry) {
        close(pipe_item->rfd);
        close(pipe_item->wfd);
    }

    free(q);
    q = NULL;
}
