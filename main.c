/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    main.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-04-17 19:04
 * updated: 2016-04-17 19:04
 ******************************************************************************/
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <libdebug.h>
#include <liblog.h>
#include "device.h"
#include "codec.h"
#include "playback.h"
#include "protocol.h"
#include "filter.h"
#include "queue.h"
#include "config.h"

struct aquila {
    struct aq_config config;
    struct queue **queue;
    struct filter_ctx **filter;
};

static struct aquila aq_instance;

int aquila_init(struct aquila *aq)
{
    struct aq_config *c = &aq->config;
    struct queue *q_src, *q_snk;
    int i;//, j;
    if (-1 == load_conf(c)) {
        loge("load_conf failed!\n");
        return -1;
    }
    if (c->filter_num <= 0) {
        logi("filter graph should not be empty\n");
        return -1;
    }
    aq->queue = CALLOC(c->filter_num, struct queue *);
    if (!aq->queue) {
        loge("malloc queue failed!\n");
        return -1;
    }
    for (i = 0; i < c->filter_num - 1; i++) {
        aq->queue[i] = queue_create();
    }
    aq->filter = CALLOC(c->filter_num, struct filter_ctx *);
    if (!aq->filter) {
        loge("malloc filter failed!\n");
        return -1;
    }

    for (i = 0; i < c->filter_num; i++) {
        if (i == 0) {
            q_src = NULL;
            q_snk = aq->queue[i];
        } else if (i < c->filter_num) {
            q_src = aq->queue[i-1];
            q_snk = aq->queue[i];
        } else {
            q_src = aq->queue[i];
            q_snk = NULL;
        }
        aq->filter[i] = filter_create(&c->filter[i], q_src, q_snk);
    }
    return 0;
}

int aquila_dispatch(struct aquila *aq)
{
    int i;
    for (i = 0; i < aq->config.filter_num; i++) {
        filter_dispatch(aq->filter[i], 0);
    }
    while (1) {
        sleep(2);
    }
    return 0;
}

void aquila_deinit(struct aquila *aq)
{
    int i;
    for (i = 0; i < aq->config.filter_num; i++) {
        filter_destroy(aq->filter[i]);
    }
}


static void sigint_handler(int sig)
{
    aquila_deinit(&aq_instance);
    exit(0);
}


void signal_init()
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sigint_handler);
}

void registre_class()
{
    device_register_all();
    playback_register_all();
    codec_register_all();
    filter_register_all();
    protocol_register_all();
}

int main(int argc, char **argv)
{
    debug_backtrace_init();
    signal_init();
    registre_class();

    memset(&aq_instance, 0, sizeof(aq_instance));
    if (-1 == aquila_init(&aq_instance)) {
        loge("aquila_init failed!\n");
        return -1;
    }
    if (-1 == aquila_dispatch(&aq_instance)) {
        loge("aquila_dispatch failed!\n");
        return -1;
    }

    aquila_deinit(&aq_instance);

    return 0;
}
