/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    queue.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-01 15:15
 * updated: 2016-05-01 15:15
 ******************************************************************************/
#ifndef _QUEUE_H_
#define _QUEUE_H_

#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <libgzf.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * QUEUE is a multi reader single writer queue, like "Y"
 * reader number is fixed
 */

struct queue_item {
    struct list_head entry;
    void *data;
    int len;
    volatile int ref_cnt;
};

struct pipe_fds {
    int rfd; //pipe fds[0]
    int wfd; //pipe fds[1]
    int used;
    struct list_head entry;
};

struct queue {
    struct list_head head;
    pthread_mutex_t lock;
    int ref_cnt;//every item ref_cnt is fixed
    int depth;//current depth
    int max_depth;
    struct list_head pipe_list;
    struct media_params media;
    char src_filter[32];
    char snk_filter[32];
};

struct queue_item *queue_item_new(void *data, int len);
void queue_item_free(struct queue_item *item);

struct queue *queue_create();
int queue_add_ref(struct queue *q);
int queue_get_available_fd(struct queue *q);
int queue_push(struct queue *q, struct queue_item *item);
struct queue_item *queue_pop(struct queue *q, int fd, int *last);
int queue_depth(struct queue *q);
void queue_destroy(struct queue *q);


#ifdef __cplusplus
}
#endif
#endif
