/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    playback_filter.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-02 18:58
 * updated: 2016-05-02 18:58
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <liblog.h>

#include "queue.h"
#include "playback.h"
#include "filter.h"
#include "common.h"

#define PLAYBACK_FILTER_SDL_RGB "sdl://rgb"
#define PLAYBACK_FILTER_SDL_YUV "sdl://yuv"
#define PLAYBACK_FILTER_SNK "snk://"

struct playback_filter_ctx {
    int seq;
    struct playback_ctx *pc;
};

static int on_playback_read(void *arg, void *in_data, int in_len,
                             void **out_data, int *out_len)
{
    struct playback_filter_ctx *da = (struct playback_filter_ctx *)arg;
    *out_len = playback_write(da->pc, in_data, in_len);
    if (*out_len == -1) {
        //loge("playback failed!\n");
    }
    da->seq++;
    logv("seq = %d, len = %d\n", da->seq, *out_len);
    return *out_len;
}

static int playback_filter_open(struct filter_ctx *fc)
{
    //const char *url = PLAYBACK_FILTER_SDL_RGB;
    //const char *url = PLAYBACK_FILTER_SDL_YUV;
    const char *url = PLAYBACK_FILTER_SNK;
    struct playback_ctx *pc = playback_open(url, &fc->media);
    if (!pc) {
        loge("open %s failed!\n", url);
        return -1;
    }
    struct playback_filter_ctx *pfc = CALLOC(1, struct playback_filter_ctx);
    if (!pfc) {
        loge("malloc failed!\n");
        goto failed;
    }
    pfc->pc = pc;
    pfc->seq = 0;
    pc->media.video.width = fc->media.video.width;
    pc->media.video.height = fc->media.video.height;
    fc->rfd = -1;
    fc->wfd = -1;
    fc->priv = pfc;
    return 0;
failed:
    if (pc) {
        playback_close(pc);
    }
    if (pfc) {
        free(pfc);
    }
    return -1;
}

static void playback_filter_close(struct filter_ctx *fc)
{
    struct playback_filter_ctx *pfc = (struct playback_filter_ctx *)fc->priv;
    if (pfc->pc) {
        playback_close(pfc->pc);
        free(pfc);
    }
}

struct filter aq_playback_filter = {
    .name = "playback",
    .open = playback_filter_open,
    .on_read = on_playback_read,
    .on_write = NULL,
    .close = playback_filter_close,
};
