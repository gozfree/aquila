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
#include <unistd.h>
#include <signal.h>
#include <libdebug.h>
#include <liblog.h>
#include "device.h"
#include "codec.h"
#include "playback.h"
#include "protocol.h"
#include "muxer.h"
#include "filter.h"
#include "queue.h"
#include "config.h"

struct aquila {
    struct aq_config config;
    struct aqueue **aqueue;
    struct filter_ctx **filter;
    bool run;
};

static struct aquila aq_instance;

int aquila_init(struct aquila *aq)
{
    struct aq_config *c = &aq->config;
    struct aqueue *q_src, *q_snk;
    int i;//, j;
    if (-1 == load_conf(c)) {
        loge("load_conf failed!\n");
        return -1;
    }
    if (c->filter_num <= 0) {
        logi("filter graph should not be empty\n");
        return -1;
    }
    aq->aqueue = CALLOC(c->filter_num, struct aqueue *);
    if (!aq->aqueue) {
        loge("malloc aqueue failed!\n");
        return -1;
    }
    for (i = 0; i < c->filter_num - 1; i++) {
        aq->aqueue[i] = aqueue_create();
    }
    aq->filter = CALLOC(c->filter_num, struct filter_ctx *);
    if (!aq->filter) {
        loge("malloc filter failed!\n");
        return -1;
    }

    for (i = 0; i < c->filter_num; i++) {
        if (i == 0) {
            q_src = NULL;
            q_snk = aq->aqueue[i];
        } else if (i < c->filter_num) {
            q_src = aq->aqueue[i-1];
            q_snk = aq->aqueue[i];
        } else {
            q_src = aq->aqueue[i];
            q_snk = NULL;
        }
        aq->filter[i] = filter_create(&c->filter[i], q_src, q_snk);
    }
    aq->run = true;
    return 0;
}

int aquila_dispatch(struct aquila *aq)
{
    int i;
    for (i = 0; i < aq->config.filter_num; i++) {
        filter_dispatch(aq->filter[i], 0);
    }
    while (aq->run) {
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
    aq->run = false;
}


static void sigint_handler(int sig)
{
    aquila_deinit(&aq_instance);
}

void signal_init()
{
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, sigint_handler);
}

void register_class()
{
    device_register_all();
    playback_register_all();
    codec_register_all();
    filter_register_all();
    protocol_register_all();
    muxer_register_all();
}

int main(int argc, char **argv)
{
    debug_backtrace_init();
    signal_init();
    register_class();

    memset(&aq_instance, 0, sizeof(aq_instance));
    if (-1 == aquila_init(&aq_instance)) {
        loge("aquila_init failed!\n");
        return -1;
    }
    if (-1 == aquila_dispatch(&aq_instance)) {
        loge("aquila_dispatch failed!\n");
        return -1;
    }
    logi("quit\n");

    return 0;
}
