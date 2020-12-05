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
#include <gear-lib/liblog.h>
#include <gear-lib/libmedia-io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "config.h"

#define DEFAULT_CONFIG_FILE	"./aquila.lua"
struct ikey_cvalue conf_map_table[] = {
    {ALSA,     "alsa"},
    {MJPEG,    "mjpeg"},
    {PLAYBACK, "playback"},
    {SDL,      "sdl"},
    {SNKFAKE,  "snkfake"},
    {UPSTREAM, "upstream"},
    {USBCAM,   "usbcam"},
    {VDEVFAKE, "vdevfake"},
    {VIDEOENC, "videoenc"},
    {VIDEODEC, "videodec"},
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
    if (!str) {
        return -1;
    }
    for (int i = 0; i < ARRAY_SIZE(conf_map_table); i++) {
        if (!strcasecmp(str, conf_map_table[i].str)) {
            return conf_map_table[i].val;
        }
    }
    return -1;
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

static void load_videocap(struct config *c, struct videocap_conf *v)
{
    const char *key = "videocap";
    strcpy(v->type.str, conf_get_string(c, key, "type"));
    v->type.val = string_to_enum(v->type.str);
    strcpy(v->device, conf_get_string(c, key, "device"));
    v->url = CALLOC(strlen(v->type.str)+ strlen(v->device) + strlen("://"), char);
    strcat(v->url, v->type.str);
    strcat(v->url, "://");
    strcat(v->url, v->device);

    v->mp.type = MEDIA_TYPE_VIDEO;
    v->mp.video.width = conf_get_int(c, key, "width");
    v->mp.video.height = conf_get_int(c, key, "height");
    strcpy(v->format, conf_get_string(c, key, "format"));
    v->mp.video.format = pixel_string_to_format(v->format);
    logi("[videocap][type] = %s\n", v->type.str);
    logi("[videocap][device] = %s\n", v->device);
    logi("[videocap][url] = %s\n", v->url);
    logi("[videocap][format] = %s\n", v->format);
    logi("[videocap][w*h] = %d*%d\n", v->mp.video.width, v->mp.video.height);
}

static void load_videoenc(struct config *c, struct videoenc_conf *v)
{
    const char *key = "videoenc";
    strcpy(v->type.str, conf_get_string(c, key, "type"));
    v->type.val = string_to_enum(v->type.str);
    v->url = CALLOC(strlen(v->type.str) + strlen("://"), char);
    strcat(v->url, v->type.str);
    strcat(v->url, "://");

    v->me.type = MEDIA_TYPE_VIDEO;
    v->me.video.type = VIDEO_CODEC_H264;
    strcpy(v->format, conf_get_string(c, key, "format"));
    v->me.video.format = pixel_string_to_format(v->format);
    v->me.video.width = conf_get_int(c, key, "width");
    v->me.video.height = conf_get_int(c, key, "height");
    v->me.video.framerate.num = 1;
    v->me.video.framerate.den = 30;

    logi("[videoenc][type] = %s\n", v->type.str);
    logi("[videocap][format] = %s\n", v->format);
    logi("[videoenc][w*h] = %d*%d\n", v->me.video.width, v->me.video.height);
}

static void load_audioenc(struct config *c, struct audioenc_conf *a)
{
    const char *key = "audioenc";
    strcpy(a->type.str, conf_get_string(c, key, "type"));
    a->type.val = string_to_enum(a->type.str);
    a->url = CALLOC(strlen(a->type.str) + strlen("://"), char);
    strcat(a->url, a->type.str);
    strcat(a->url, "://");

    logi("[audioenc][type] = %s\n", a->type.str);
}


static void load_videodec(struct config *c, struct videodec_conf *v)
{
    const char *key = "videodec";
    strcpy(v->type.str, conf_get_string(c, key, "type"));
    v->type.val = string_to_enum(v->type.str);
    v->url = CALLOC(strlen(v->type.str) + strlen("://"), char);
    strcat(v->url, v->type.str);
    strcat(v->url, "://");

    logi("[videodec][type] = %s\n", v->type.str);
}

static void load_audiocap(struct config *c, struct audiocap_conf *a)
{
    const char *key = "audiocap";
    strcpy(a->type.str, conf_get_string(c, key, "type"));
    a->type.val = string_to_enum(a->type.str);
    strcpy(a->device, conf_get_string(c, key, "device"));
    a->url = CALLOC(strlen(a->type.str)+ strlen(a->device) + strlen("://"), char);
    strcat(a->url, a->type.str);
    strcat(a->url, "://");
    strcat(a->url, a->device);

    a->mp.type = MEDIA_TYPE_AUDIO;
    a->mp.audio.sample_rate = conf_get_int(c, key, "sample_rate");
    a->mp.audio.channels = conf_get_int(c, key, "channels");

    strcpy(a->format, conf_get_string(c, key, "format"));
    a->mp.audio.format = sample_string_to_format(a->format);
    logi("[audiocap][type] = %s\n", a->type.str);
    logi("[audiocap][device] = %s\n", a->device);
    logi("[audiocap][url] = %s\n", a->url);
    logi("[audiocap][format] = %s\n", a->format);
    logi("[audiocap][sample_rate] = %d\n", a->sample_rate);
    logi("[audiocap][channels] = %d\n", a->channels);
}


static void load_upstream(struct config *c, struct upstream_conf *u)
{
    const char *key = "upstream";
    strcpy(u->type.str, conf_get_string(c, key, "type"));
    u->url = strdup(conf_get_string(c, key, "url"));
    u->type.val = string_to_enum(u->type.str);
    u->port = conf_get_int(c, key, "port");
    logi("[upstream][type] = %s\n", u->type.str);
    logi("[upstream][port] = %d\n", u->port);
    logi("[upstream][url] = %s\n", u->url);
}

static void load_record(struct config *c, struct record_conf *r)
{
    const char *key = "record";
    strcpy(r->type.str, conf_get_string(c, key, "type"));
    r->type.val = string_to_enum(r->type.str);
    strcat(r->url, r->type.str);
    strcat(r->url, "://");
    strcat(r->url, conf_get_string(c, "record", "file"));
    logi("[record][type] = %s\n", r->type.str);
    logi("[record][url] = %s\n", r->url);
}

static void load_playback(struct config *c, struct playback_conf *p)
{
    const char *key = "playback";
    strcpy(p->type.str, conf_get_string(c, key, "type"));
    p->type.val = string_to_enum(p->type.str);
    strcpy(p->device, conf_get_string(c, key, "device"));
    strcpy(p->format, conf_get_string(c, key, "format"));
    p->url = CALLOC(strlen(p->type.str) + strlen(p->format) + strlen("://"), char);
    strcat(p->url, p->type.str);
    strcat(p->url, "://");
    strcat(p->url, p->format);
    p->me.video.width = conf_get_int(c, key, "width");
    p->me.video.height = conf_get_int(c, key, "height");

    logi("[playback][type] = %s\n", p->type.str);
    logi("[playback][url] = %s\n", p->url);
    logi("[playback][w*h] = %d*%d\n", p->me.video.width, p->me.video.height);
}

static void load_remotectrl(struct config *c, struct remotectrl_conf *r)
{
    const char *key = "remotectrl";
    r->port = conf_get_int(c, key, "port");
    logi("[remotectrl][port] = %d\n", r->port);
}

static void load_filter(struct aq_config *c)
{
    int i;
    const char *key = "filters";
    c->filter_num = conf_get_length(c->conf, key);
    c->filter = CALLOC(c->filter_num, struct filter_conf);

    for (i = 0; i < c->filter_num; i++) {
        strcpy(c->filter[i].type.str, conf_get_string(c->conf, key, i+1));
        c->filter[i].type.val = string_to_enum(c->filter[i].type.str);
        logi("[filter][%d][type] = %s\n", i, c->filter[i].type.str);
        switch (c->filter[i].type.val) {
        case VIDEOCAP:
            c->filter[i].url = c->videocap.url;
            memcpy(&c->filter[i].videocap, &c->videocap, sizeof(struct videocap_conf));
            break;
        case PLAYBACK:
            c->filter[i].url = c->playback.url;
            memcpy(&c->filter[i].playback, &c->playback, sizeof(struct playback_conf));
            break;
        case VIDEOENC:
            c->filter[i].url = c->videoenc.url;
            memcpy(&c->filter[i].videoenc, &c->videoenc, sizeof(struct videoenc_conf));
            break;
        case VIDEODEC:
            c->filter[i].url = c->videodec.url;
            memcpy(&c->filter[i].videodec, &c->videodec, sizeof(struct videodec_conf));
            break;
        case UPSTREAM:
            c->filter[i].url = c->upstream.url;
            memcpy(&c->filter[i].upstream, &c->upstream, sizeof(struct upstream_conf));
            break;
        case RECORD:
            c->filter[i].url = c->record.url;
            memcpy(&c->filter[i].record, &c->record, sizeof(struct record_conf));
            break;
        case REMOTECTRL:
            c->filter[i].url = "rpcd://xxx";
            memcpy(&c->filter[i].remotectrl, &c->remotectrl, sizeof(struct remotectrl_conf));
            break;
        default :
            loge("unsupport filter type %d, %s\n",
                 c->filter[i].type.val, c->filter[i].type.str);
            break;
        }
    }
}

static void load_graph(struct aq_config *c)
{
    int i;
    c->graph_num = conf_get_length(c->conf, "graphs");
    c->graph = CALLOC(c->graph_num, struct graph_conf);

    logi("[graph][num] = %d\n", c->graph_num);
    for (i = 0; i < c->graph_num; i++) {
        strcpy(c->graph[i].source, conf_get_string(c->conf, "graphs", i+1, "source"));
        strcpy(c->graph[i].sink, conf_get_string(c->conf, "graphs", i+1, "sink"));
        logi("[graph][%d][source] = %s\n", i, c->graph[i].source);
        logi("[graph][%d][sink] = %s\n", i, c->graph[i].sink);
    }
}

int load_conf(struct aq_config *c)
{
    c->conf = conf_load(DEFAULT_CONFIG_FILE);
    if (!c->conf) {
        loge("conf_load failed!\n");
        return -1;
    }
    load_videocap(c->conf, &c->videocap);
    load_audiocap(c->conf, &c->audiocap);
    load_videoenc(c->conf, &c->videoenc);
    load_audioenc(c->conf, &c->audioenc);
    load_videodec(c->conf, &c->videodec);
    load_record(c->conf, &c->record);
    load_upstream(c->conf, &c->upstream);
    load_playback(c->conf, &c->playback);
    load_remotectrl(c->conf, &c->remotectrl);
    load_filter(c);
    load_graph(c);
    return 0;
}

void unload_conf(struct aq_config *c)
{
    conf_unload(c->conf);
}
