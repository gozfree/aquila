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
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <gear-lib/libconfig.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct videocap_conf {
    struct ikey_cvalue type;
    char device[256];
    char format[32];
    struct media_producer mp;
    char *url;
};

struct audiocap_conf {
    struct ikey_cvalue type;
    char device[256];
    char format[32];
    struct media_producer mp;
    int sample_rate;
    int channels;
    char *url;
};

struct videoenc_conf {
    struct ikey_cvalue type;
    char format[32];
    struct media_encoder me;
    char *url;
};

struct audioenc_conf {
    struct ikey_cvalue type;
    char *url;
};


struct videodec_conf {
    struct ikey_cvalue type;
    char *url;
};

struct upstream_conf {
    struct ikey_cvalue type;
    char *url;
    uint16_t port;
    struct media_encoder me;
};

struct playback_conf {
    struct ikey_cvalue type;
    char device[256];
    char format[32];
    struct media_encoder me;
    char *url;
};

struct record_conf {
    struct ikey_cvalue type;
    struct media_encoder me;
    char url[128];
};

struct remotectrl_conf {
    int port;
    struct media_encoder me;
};

struct filter_conf {
    struct ikey_cvalue type;
    char *url;
    union {
        struct videocap_conf videocap;
        struct videoenc_conf videoenc;
        struct videodec_conf videodec;
        struct record_conf record;
        struct upstream_conf upstream;
        struct playback_conf playback;
        struct remotectrl_conf remotectrl;
    };
};

struct graph_conf {
    char source[32];
    char sink[32];
};


struct aq_config {
    struct videocap_conf   videocap;
    struct audiocap_conf   audiocap;
    struct videoenc_conf   videoenc;
    struct audioenc_conf   audioenc;
    struct videodec_conf   videodec;
    struct record_conf     record;
    struct upstream_conf   upstream;
    struct playback_conf   playback;
    struct remotectrl_conf remotectrl;

    int                    filter_num;
    struct filter_conf    *filter;
    int                    graph_num;
    struct graph_conf     *graph;
    struct config         *conf;
};

int load_conf(struct aq_config *c);
void unload_conf(struct aq_config *c);


#ifdef __cplusplus
}
#endif
#endif
