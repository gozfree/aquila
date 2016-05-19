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
#include "config.h"

#define PLAYBACK_FILTER_SDL_RGB "sdl://rgb"
#define PLAYBACK_FILTER_SDL_YUV "sdl://yuv"
#define PLAYBACK_FILTER_SNK "snkfake://"

struct playback_filter_ctx {
    int seq;
    struct playback_ctx *pc;
    struct playback_conf *conf;
};

static int on_playback_read(struct filter_ctx *fc, struct iovec *in, struct iovec *out)
{
    struct playback_filter_ctx *da = (struct playback_filter_ctx *)fc->priv;
    int ret = playback_write(da->pc, in->iov_base, in->iov_len);
    if (ret == -1) {
        //loge("playback failed!\n");
    }
    da->seq++;
    out->iov_len = ret;
    logv("seq = %d, len = %d\n", da->seq, ret);
    return ret;
}

static int playback_filter_open(struct filter_ctx *fc)
{
    struct filter_conf *fconf = (struct filter_conf *)fc->config;
    struct playback_filter_ctx *pfc = CALLOC(1, struct playback_filter_ctx);
    if (!pfc) {
        loge("malloc failed!\n");
        return -1;
    }
    pfc->conf = &fconf->conf.playback;
    struct playback_ctx *pc = playback_open(fc->url, &pfc->conf->param);
    if (!pc) {
        loge("open %s failed!\n", fc->url);
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
    .name     = "playback",
    .open     = playback_filter_open,
    .on_read  = on_playback_read,
    .on_write = NULL,
    .close    = playback_filter_close,
};
