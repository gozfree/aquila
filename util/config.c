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
#include <liblog.h>
#include <libconfig.h>
#include "config.h"

#define DEFAULT_CONFIG_FILE	"aquila.lua"

struct config_map {
    int index;
    char name[32];
};

enum conf_map_index {
    V4L2 = 0,
    VDEVFAKE,
    ALSA,
    MJPEG,
    X264,
    SDL,
    SNKFAKE,
    VIDEOCAP,
    VENCODE,
    PLAYBACK,
};

struct config_map conf_map_list[] = {
    {V4L2,     "v4l2"},
    {VDEVFAKE, "vdevfake"},
    {ALSA,     "alsa"},
    {MJPEG,    "mjpeg"},
    {X264,     "x264"},
    {SDL,      "sdl"},
    {SNKFAKE,  "snkfake"},
    {VIDEOCAP, "videocap"},
    {VENCODE,  "vencode"},
    {PLAYBACK, "playback"},

};

int load_conf(struct aq_config *aqc)
{
    struct config *c = conf_load(CONFIG_LUA, DEFAULT_CONFIG_FILE);
    if (!c) {
        loge("failed to conf_load %s\n", DEFAULT_CONFIG_FILE);
        return -1;
    }
    strcpy(aqc->videocap.type, conf_get_string(c, "videocap", "type"));
    aqc->videocap.width = conf_get_int(c, "videocap", "width");
    aqc->videocap.height = conf_get_int(c, "videocap", "height");

    strcpy(aqc->vencode.type, conf_get_string(c, "vencode", "type"));

    strcpy(aqc->playback.type, conf_get_string(c, "playback", "type"));
#if 1
    logi("[videocap][type] = %s\n", aqc->videocap.type);
    logi("[videocap][width] = %d\n", aqc->videocap.width);
    logi("[videocap][height] = %d\n", aqc->videocap.height);
    logi("[vencode][type] = %s\n", aqc->vencode.type);
    logi("[playback][type] = %s\n", aqc->playback.type);
#endif

    conf_unload(c);
    return 0;
}

