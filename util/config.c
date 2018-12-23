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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>
#include <libmacro.h>
#include <liblog.h>
#include <libgconfig/libgconfig.h>
#include "common.h"
#include "config.h"

#define DEFAULT_CONFIG_FILE	"aquila.lua"
struct ikey_cvalue conf_map_table[] = {
    {ALSA,     "alsa"},
    {MJPEG,    "mjpeg"},
    {PLAYBACK, "playback"},
    {SDL,      "sdl"},
    {SNKFAKE,  "snkfake"},
    {UPSTREAM, "upstream"},
    {V4L2,     "v4l2"},
    {VDEVFAKE, "vdevfake"},
    {VENCODE,  "vencode"},
    {VDECODE,  "vdecode"},
    {VIDEOCAP, "videocap"},
    {RECORD,   "record"},
    {REMOTECTRL,"remotectrl"},
    {X264,     "x264"},
    {H264ENC,  "h264enc"},
    {YUV420,   "yuv420"},
    {YUV422P,  "yuv422p"},
    {UNKNOWN,  "unknown"},
};


static int string_to_enum(char *str)
{
    int ret;
    if (!str) {
        return -1;
    }
    for (int i = 0; i < SIZEOF(conf_map_table); i++) {
        if (!strcasecmp(str, conf_map_table[i].str)) {
            return conf_map_table[i].val;
        }
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

static void load_videocap(struct aq_config *c)
{
    strcpy(c->videocap.type.str, (*c->conf)["videocap"]["type"].get<string>("").c_str());
    c->videocap.type.val = string_to_enum(c->videocap.type.str);
    strcpy(c->videocap.device, (*c->conf)["videocap"]["device"].get<string>("").c_str());
    c->videocap.url = CALLOC(strlen(c->videocap.type.str)+ strlen(c->videocap.device) + strlen("://"), char);
    strcat(c->videocap.url, c->videocap.type.str);
    strcat(c->videocap.url, "://");
    strcat(c->videocap.url, c->videocap.device);
    c->videocap.param.video.width = (*c->conf)["videocap"]["width"].get<int>(0);
    c->videocap.param.video.height = (*c->conf)["videocap"]["height"].get<int>(0);
    strcpy(c->videocap.format, (*c->conf)["videocap"]["format"].get<string>("").c_str());
    c->videocap.param.video.pix_fmt = string_to_enum(c->videocap.format);
    logi("[videocap][type] = %s\n", c->videocap.type.str);
    logi("[videocap][device] = %s\n", c->videocap.device);
    logi("[videocap][url] = %s\n", c->videocap.url);
    logi("[videocap][format] = %s\n", c->videocap.format);
    logi("[videocap][w*h] = %d*%d\n", c->videocap.param.video.width, c->videocap.param.video.height);
}

static void load_vencode(struct aq_config *c)
{
    strcpy(c->vencode.type.str, (*c->conf)["vencode"]["type"].get<string>("").c_str());
    c->vencode.type.val = string_to_enum(c->vencode.type.str);
    c->vencode.url = CALLOC(strlen(c->vencode.type.str) + strlen("://"), char);
    strcat(c->vencode.url, c->vencode.type.str);
    strcat(c->vencode.url, "://");

    logi("[vencode][type] = %s\n", c->vencode.type.str);
}

static void load_vdecode(struct aq_config *c)
{
    strcpy(c->vdecode.type.str, (*c->conf)["vdecode"]["type"].get<string>("").c_str());
    c->vdecode.type.val = string_to_enum(c->vdecode.type.str);
    c->vdecode.url = CALLOC(strlen(c->vdecode.type.str) + strlen("://"), char);
    strcat(c->vdecode.url, c->vdecode.type.str);
    strcat(c->vdecode.url, "://");

    logi("[vdecode][type] = %s\n", c->vdecode.type.str);
}

static void load_upstream(struct aq_config *c)
{
    strcpy(c->upstream.type.str, (*c->conf)["upstream"]["type"].get<string>("").c_str());
    c->upstream.url = strdup((*c->conf)["upstream"]["url"].get<string>("").c_str());
    c->upstream.type.val = string_to_enum(c->upstream.type.str);
    c->upstream.port = (*c->conf)["upstream"]["port"].get<int>(0);
    logi("[upstream][type] = %s\n", c->upstream.type.str);
    logi("[upstream][url] = %s\n", c->upstream.url);
}

static void load_record(struct aq_config *c)
{
    strcpy(c->record.type.str, (*c->conf)["record"]["type"].get<string>("").c_str());
    c->record.type.val = string_to_enum(c->record.type.str);
    strcat(c->record.url, c->record.type.str);
    strcat(c->record.url, "://");
    strcat(c->record.url, (*c->conf)["record"]["file"].get<string>("").c_str());
    logi("[record][type] = %s\n", c->record.type.str);
    logi("[record][url] = %s\n", c->record.url);
}


static void load_playback(struct aq_config *c)
{
    strcpy(c->playback.type.str, (*c->conf)["playback"]["type"].get<string>("").c_str());
    c->playback.type.val = string_to_enum(c->playback.type.str);
    strcpy(c->playback.device, (*c->conf)["playback"]["device"].get<string>("").c_str());
    strcpy(c->playback.format, (*c->conf)["playback"]["format"].get<string>("").c_str());
    c->playback.url = CALLOC(strlen(c->playback.type.str) + strlen(c->playback.format) + strlen("://"), char);
    strcat(c->playback.url, c->playback.type.str);
    strcat(c->playback.url, "://");
    strcat(c->playback.url, c->playback.format);
    //c->playback.param.video.width = conf_get_int(c, "playback", "width");
    //c->playback.param.video.height = conf_get_int(c, "playback", "height");

    logi("[playback][type] = %s\n", c->playback.type.str);
    logi("[playback][url] = %s\n", c->playback.url);
    logi("[playback][w*h] = %d*%d\n", c->playback.param.video.width, c->playback.param.video.height);
}

static void load_remotectrl(struct aq_config *c)
{
    c->remotectrl.port = (*c->conf)["remotectrl"]["port"].get<int>(0);
    logi("[remotectrl][port] = %s\n", c->record.type.str);
}


static void load_filter(struct aq_config *c)
{
    int i;
    c->filter_num = (*c->conf)["filters"].length();
    c->filter = CALLOC(c->filter_num, struct filter_conf);

    for (i = 0; i < c->filter_num; i++) {
        strcpy(c->filter[i].type.str, (*c->conf)["filters"][i+1].get<string>("").c_str());
        c->filter[i].type.val = string_to_enum(c->filter[i].type.str);
        logi("[filter][%d][type] = %s\n", i, c->filter[i].type.str);
        switch (c->filter[i].type.val) {
        case VIDEOCAP:
            c->filter[i].url = c->videocap.url;
            memcpy(&c->filter[i].conf.videocap, &c->videocap, sizeof(struct videocap_conf));
            break;
        case PLAYBACK:
            c->filter[i].url = c->playback.url;
            memcpy(&c->filter[i].conf.playback, &c->playback, sizeof(struct playback_conf));
            break;
        case VENCODE:
            c->filter[i].url = c->vencode.url;
            break;
        case VDECODE:
            c->filter[i].url = c->vdecode.url;
            break;
        case UPSTREAM:
            c->filter[i].url = c->upstream.url;
            break;
        case RECORD:
            c->filter[i].url = c->record.url;
            break;
        case REMOTECTRL:
            c->filter[i].url = "rpcd://xxx";
            break;
        default :
            loge("unsupport filter type %s\n", c->filter[i].type.str);
            break;
        }
    }
}

static void load_graph(struct aq_config *c)
{
    int i;
    c->graph_num = (*c->conf)["graphs"].length();
    c->graph = CALLOC(c->graph_num, struct graph_conf);

    logi("[graph][num] = %d\n", c->graph_num);
    for (i = 0; i < c->graph_num; i++) {
        strcpy(c->graph[i].source, (*c->conf)["graphs"][i+1]["source"].get<string>("").c_str());
        strcpy(c->graph[i].sink, (*c->conf)["graphs"][i+1]["sink"].get<string>("").c_str());
        logi("[graph][%d][source] = %s\n", i, c->graph[i].source);
        logi("[graph][%d][sink] = %s\n", i, c->graph[i].sink);
    }
}

int load_conf(struct aq_config *c)
{
    c->conf = Config::create(DEFAULT_CONFIG_FILE);
    if (!c->conf) {
        loge("failed to conf_load %s\n", DEFAULT_CONFIG_FILE);
        return -1;
    }
    load_videocap(c);
    load_vencode(c);
    load_vdecode(c);
    load_record(c);
    load_upstream(c);
    load_playback(c);
    load_remotectrl(c);
    load_filter(c);
    load_graph(c);
    return 0;
}

void unload_conf(struct aq_config *c)
{
    c->conf->destroy();
}
