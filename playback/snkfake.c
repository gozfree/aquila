/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    snkfake.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-04 01:55
 * updated: 2016-05-04 01:55
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgzf.h>
#include <liblog.h>

#include "playback.h"
#include "common.h"

struct snkfake_ctx {
    int index;
    int width;
    int height;
};

static int snk_open(struct playback_ctx *pc, const char *type, struct media_params *format)
{
    struct snkfake_ctx *c = CALLOC(1, struct snkfake_ctx);
    if (!c) {
        loge("malloc snkfake_ctx failed!\n");
        return -1;
    }
    c->width = format->video.width;
    c->height = format->video.height;
    c->index = 0;
    pc->priv = c;
    logd("snk_open sucess\n");
    return 0;
}

static int snk_read(struct playback_ctx *pc, void *buf, int len)
{
    logi("snk_read sucess\n");
    return 0;
}

static int snk_write(struct playback_ctx *pc, void *buf, int len)
{
    struct snkfake_ctx *c = (struct snkfake_ctx *)pc->priv;
    char filename[64] = {0};
    int fd = -1;
    snprintf(filename, sizeof(filename), "%dx%d_%02d.jpg",
             c->width, c->height, ++(c->index));
    if ((fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0777)) < 0) {
        loge("failed to open %s\n", filename);
        return -1;
    }
    if (write(fd, buf, len) < 0) {
        loge("failed to sava data into file\n");
    }
    logi("write %s success\n", filename);
    close(fd);
    return 0;

}

static void snk_close(struct playback_ctx *pc)
{

}
struct playback aq_snk_playback = {
    .name = "snk",
    .open = snk_open,
    .read = snk_read,
    .write = snk_write,
    .close = snk_close,
};
