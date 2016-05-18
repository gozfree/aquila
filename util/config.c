/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    config.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-16 11:24
 * updated: 2016-05-16 11:24
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgzf.h>
#include <liblog.h>
#include <libconfig.h>
#include "common.h"
#include "config.h"

#define DEFAULT_CONFIG_FILE	"aquila.lua"
struct ikey_cvalue conf_map_table[] = {
    {ALSA,     "alsa"},
    {MJPEG,    "mjpeg"},
    {PLAYBACK, "playback"},
    {SDL,      "sdl"},
    {SNKFAKE,  "snkfake"},
    {V4L2,     "v4l2"},
    {VDEVFAKE, "vdevfake"},
    {VENCODE,  "vencode"},
    {VIDEOCAP, "videocap"},
    {X264,     "x264"},
    {YUV420,   "yuv420"},
    {YUV422,   "yuv422"},
    {UNKNOWN,  "unknown"},
};


static int string_to_enum(char *str)
{
    int ret;
    if (!str) {
        return -1;
    }
    if (!strcasecmp(str, "alsa")) {
        ret = ALSA;
    } else if (!strcasecmp(str, "mjpeg")) {
        ret = MJPEG;
    } else if (!strcasecmp(str, "playback")) {
        ret = PLAYBACK;
    } else if (!strcasecmp(str, "sdl")) {
        ret = SDL;
    } else if (!strcasecmp(str, "snkfake")) {
        ret = SNKFAKE;
    } else if (!strcasecmp(str, "v4l2")) {
        ret = V4L2;
    } else if (!strcasecmp(str, "vencode")) {
        ret = VENCODE;
    } else if (!strcasecmp(str, "vdevfake")) {
        ret = VDEVFAKE;
    } else if (!strcasecmp(str, "videocap")) {
        ret = VIDEOCAP;
    } else if (!strcasecmp(str, "x264")) {
        ret = X264;
    } else {
        ret = UNKNOWN;
    }
    return ret;
}

#if 0
static char *enum_to_string(int key)
{
    if (key > SIZEOF(conf_map_table)) {
        return NULL;
    }
    return conf_map_table[key].str;
}
#endif

static void load_videocap(struct aq_config *aqc)
{
    struct config *c = aqc->config;
    strcpy(aqc->videocap.type.str, conf_get_string(c, "videocap", "type"));
    aqc->videocap.type.val = string_to_enum(aqc->videocap.type.str);
    strcpy(aqc->videocap.device, conf_get_string(c, "videocap", "device"));
    aqc->videocap.url = CALLOC(strlen(aqc->videocap.type.str)+ strlen(aqc->videocap.device) + strlen("://"), char);
    strcat(aqc->videocap.url, aqc->videocap.type.str);
    strcat(aqc->videocap.url, "://");
    strcat(aqc->videocap.url, aqc->videocap.device);
    aqc->videocap.param.video.width = conf_get_int(c, "videocap", "width");
    aqc->videocap.param.video.height = conf_get_int(c, "videocap", "height");
    strcpy(aqc->videocap.format, conf_get_string(c, "videocap", "format"));
    aqc->videocap.param.video.pix_fmt = string_to_enum(aqc->videocap.format);
    logi("[videocap][type] = %s\n", aqc->videocap.type.str);
    logi("[videocap][device] = %s\n", aqc->videocap.device);
    logi("[videocap][url] = %s\n", aqc->videocap.url);
    logi("[videocap][format] = %s\n", aqc->videocap.format);
    logi("[videocap][width] = %d\n", aqc->videocap.param.video.width);
    logi("[videocap][height] = %d\n", aqc->videocap.param.video.height);
}

static void load_vencode(struct aq_config *aqc)
{
    struct config *c = aqc->config;
    strcpy(aqc->vencode.type.str, conf_get_string(c, "vencode", "type"));
    aqc->vencode.type.val = string_to_enum(aqc->vencode.type.str);
    aqc->vencode.url = CALLOC(strlen(aqc->vencode.type.str) + strlen("://"), char);
    strcat(aqc->vencode.url, aqc->vencode.type.str);
    strcat(aqc->vencode.url, "://");

    logi("[vencode][type] = %s\n", aqc->vencode.type.str);
    logi("[vencode][url] = %s\n", aqc->vencode.url);
}

static void load_playback(struct aq_config *aqc)
{
    struct config *c = aqc->config;
    strcpy(aqc->playback.type.str, conf_get_string(c, "playback", "type"));
    aqc->playback.type.val = string_to_enum(aqc->playback.type.str);
    strcpy(aqc->playback.device, conf_get_string(c, "playback", "device"));
    strcpy(aqc->playback.format, conf_get_string(c, "playback", "format"));
    aqc->playback.url = CALLOC(strlen(aqc->playback.type.str) + strlen(aqc->playback.format) + strlen("://"), char);
    strcat(aqc->playback.url, aqc->playback.type.str);
    strcat(aqc->playback.url, "://");
    strcat(aqc->playback.url, aqc->playback.format);
    aqc->playback.param.video.width = conf_get_int(c, "playback", "width");
    aqc->playback.param.video.height = conf_get_int(c, "playback", "height");

    logi("[playback][type] = %s\n", aqc->playback.type.str);
    logi("[playback][url] = %s\n", aqc->playback.url);
    logi("[playback][width] = %d\n", aqc->playback.param.video.width);
    logi("[playback][height] = %d\n", aqc->playback.param.video.height);
}

static void load_filter(struct aq_config *aqc)
{
    int i;
    char filter[32];
    struct config *c = aqc->config;
    aqc->filter_num = conf_get_int(c, "filter", "num");
    aqc->filter = CALLOC(aqc->filter_num, struct filter_conf);

    logi("[filter][num] = %d\n", aqc->filter_num);
    for (i = 0; i < aqc->filter_num; i++) {
        memset(&filter, 0, sizeof(filter));
        snprintf(filter, sizeof(filter), "type%d", i);
        strcpy(aqc->filter[i].type.str, conf_get_string(c, "filter", filter));
        aqc->filter[i].type.val = string_to_enum(aqc->filter[i].type.str);
        logi("[filter][%d][type] = %s\n", i, aqc->filter[i].type.str);
        switch (aqc->filter[i].type.val) {
        case VIDEOCAP:
            aqc->filter[i].url = aqc->videocap.url;
            memcpy(&aqc->filter[i].conf.videocap, &aqc->videocap, sizeof(struct videocap_conf));
            break;
        case PLAYBACK:
            aqc->filter[i].url = aqc->playback.url;
            memcpy(&aqc->filter[i].conf.playback, &aqc->playback, sizeof(struct playback_conf));
            break;
        case VENCODE:
            aqc->filter[i].url = aqc->vencode.url;
            break;
        default :
            loge("unsupport filter type %s\n", aqc->filter[i].type.str)
            break;
        }
    }

}

static void load_graph(struct aq_config *aqc)
{
    int i;
    char graphn[32];
    struct config *c = aqc->config;
    aqc->graph_num = conf_get_int(c, "graph", "num");
    aqc->graph = CALLOC(aqc->graph_num, struct graph_conf);

    logi("[graph][num] = %d\n", aqc->graph_num);
    for (i = 0; i < aqc->graph_num; i++) {
        memset(&graphn, 0, sizeof(graphn));
        snprintf(graphn, sizeof(graphn), "graph%d", i);
        strcpy(aqc->graph[i].source, conf_get_string(c, "graph", graphn, "source"));
        strcpy(aqc->graph[i].sink, conf_get_string(c, "graph", graphn, "sink"));
        logi("[graph][%d][source] = %s\n", i, aqc->graph[i].source);
        logi("[graph][%d][sink] = %s\n", i, aqc->graph[i].sink);
    }
}



int load_conf(struct aq_config *aqc)
{
    struct config *c = NULL;
    c = aqc->config = conf_load(CONFIG_LUA, DEFAULT_CONFIG_FILE);
    if (!c) {
        loge("failed to conf_load %s\n", DEFAULT_CONFIG_FILE);
        return -1;
    }

    load_videocap(aqc);
    load_vencode(aqc);
    load_playback(aqc);
    load_filter(aqc);
    load_graph(aqc);
    return 0;
}

void unload_conf(struct aq_config *aqc)
{
    if (aqc->config) {
        conf_unload(aqc->config);
    }
}
