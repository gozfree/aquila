/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    config.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-16 11:27
 * updated: 2016-05-16 11:27
 ******************************************************************************/
#ifndef _CONFIG_H_
#define _CONFIG_H_

#include <libconfig.h>
#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif

struct videocap_conf {
    struct ikey_cvalue type;
    char device[256];
    char format[32];
    struct media_params param;
    char *url;
};

struct vencode_conf {
    struct ikey_cvalue type;
    char *url;
};

struct vdecode_conf {
    struct ikey_cvalue type;
    char *url;
};

struct upstream_conf {
    struct ikey_cvalue type;
    char *url;
    uint16_t port;
};

struct playback_conf {
    struct ikey_cvalue type;
    char device[256];
    char format[32];
    struct media_params param;
    char *url;
};

struct filter_conf {
    struct ikey_cvalue type;
    char *url;
    union {
        struct videocap_conf videocap;
        struct vencode_conf vencode;
        struct vdecode_conf vdecode;
        struct upstream_conf upstream;
        struct playback_conf playback;
    } conf;
};

struct graph_conf {
    char source[32];
    char sink[32];
};


struct aq_config {
    struct config *config;
    struct videocap_conf videocap;
    struct vencode_conf vencode;
    struct vdecode_conf vdecode;
    struct upstream_conf upstream;
    struct playback_conf playback;

    int filter_num;
    struct filter_conf *filter;
    int graph_num;
    struct graph_conf *graph;
};

int load_conf();


#ifdef __cplusplus
}
#endif
#endif
