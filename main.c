/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    main.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-04-17 19:04
 * updated: 2016-04-17 19:04
 ******************************************************************************/
#include <unistd.h>
#include <signal.h>
#include <liblog.h>
#include "device.h"
#include "codec.h"
#include "playback.h"
#include "filter.h"
#include "queue.h"

struct aquila {
    struct filter_ctx *videocap_filter;
    struct filter_ctx *vencode_filter;
    struct filter_ctx *playback_filter;
};

static struct aquila aq_instance;

int aquila_init(struct aquila *aq)
{
    struct queue *q1 = queue_create();
    struct queue *q2 = queue_create();

    aq->videocap_filter = filter_create("videocap", NULL, q1);
    if (!aq->videocap_filter) {
        loge("filter videocap create failed!\n");
        return -1;
    }
    aq->vencode_filter = filter_create("vencode", q1, q2);
    if (!aq->vencode_filter) {
        loge("filter vencode create failed!\n");
        return -1;
    }
    aq->playback_filter = filter_create("playback", q2, NULL);
    if (!aq->playback_filter) {
        loge("filter playback create failed!\n");
        return -1;
    }
    return 0;
}

int aquila_dispatch(struct aquila *aq)
{
    filter_dispatch(aq->videocap_filter, 0);
    filter_dispatch(aq->vencode_filter, 0);
    filter_dispatch(aq->playback_filter, 0);
    while (1) {
        sleep(2);
    }
    return 0;
}

void aquila_deinit(struct aquila *aq)
{
    filter_destroy(aq->videocap_filter);
    filter_destroy(aq->playback_filter);
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


int main(int argc, char **argv)
{
    signal_init();
    device_register_all();
    playback_register_all();
    codec_register_all();
    filter_register_all();

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
