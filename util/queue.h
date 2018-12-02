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
#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <sys/uio.h>
#include <pthread.h>
#include <libmacro.h>


#ifdef __cplusplus
extern "C" {
#endif

enum aqueue_mode {
    QUEUE_FULL_FLUSH = 0,
    QUEUE_FULL_RING,
};


/*
 * QUEUE is a multi reader single writer aqueue, like "Y"
 * reader number is fixed
 */

struct aitem {
    struct iovec data;
    struct list_head entry;
    struct iovec opaque;
    volatile int ref_cnt;
};

struct pipe_fds {
    int rfd; //pipe fds[0]
    int wfd; //pipe fds[1]
    int used;
    struct list_head entry;
};

typedef void *(alloc_hook)(void *data, size_t len);
typedef void (free_hook)(void *data);

struct aqueue {
    struct list_head head;
    int depth;
    int max_depth;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    enum aqueue_mode mode;
    alloc_hook *alloc_hook;
    free_hook *free_hook;
    int ref_cnt;//every aitem ref_cnt is fixed
    struct list_head pipe_list;
    struct iovec opaque;
};

struct aitem *aitem_alloc(struct aqueue *q, void *data, size_t len);
void aitem_free(struct aqueue *q, struct aitem *aitem);

struct aqueue *aqueue_create();
void aqueue_destroy(struct aqueue *q);
int aqueue_set_depth(struct aqueue *q, int depth);
int aqueue_get_depth(struct aqueue *q);
int aqueue_set_mode(struct aqueue *q, enum aqueue_mode mode);
int aqueue_set_hook(struct aqueue *q, alloc_hook *alloc_cb, free_hook *free_cb);
struct aitem *aqueue_pop(struct aqueue *q, int fd, int *last);
int aqueue_add_ref(struct aqueue *q);
int aqueue_get_available_fd(struct aqueue *q);
int aqueue_push(struct aqueue *q, struct aitem *aitem);
int aqueue_flush(struct aqueue *q);

#ifdef __cplusplus
}
#endif
#endif
