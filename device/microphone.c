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
#include <gear-lib/libuac.h>
#include <gear-lib/libmedia-io.h>
#include <gear-lib/libmacro.h>
#include <gear-lib/liblog.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "device.h"
#include "common.h"

struct mic_ctx {
    struct uac_config conf;
    struct uac_ctx *uac;
    char *name;
    int on_read_fd;
    int on_write_fd;
};

static int on_frame(struct uac_ctx *c, struct audio_frame *frm)
{
#if 0
    printf("frm[%" PRIu64 "] cnt=%d size=%" PRIu64 ", ts=%" PRIu64 " ms\n",
           frm->frame_id, frm->frames, frm->total_size, frm->timestamp/1000000);
#endif
    return 0;
}

static int mic_open(struct device_ctx *dc, const char *dev, struct media_encoder *ma)
{
    int fds[2] = {0};
    char notify = '1';
    struct mic_ctx *mc = CALLOC(1, struct mic_ctx);
    if (!mc) {
        loge("malloc mic_ctx failed!\n");
        return -1;
    }
    if (pipe(fds)) {
        loge("create pipe failed(%d): %s\n", errno, strerror(errno));
        goto failed;
    }
    mc->on_read_fd = fds[0];
    mc->on_write_fd = fds[1];
    logd("pipe: rfd = %d, wfd = %d\n", mc->on_read_fd, mc->on_write_fd);

    mc->uac = uac_open(dev, &mc->conf);
    if (!mc->uac) {
        loge("uac_open failed!\n");
        goto failed;
    }
    if (uac_start_stream(mc->uac, on_frame)) {
        loge("uac start stream failed!\n");
        goto failed;
    }
    mc->name = strdup(dev);
    logd("open %s format:%s sample_rate: %d, channel: %d\n",
          dev, sample_format_name(mc->uac->conf.format),
          mc->uac->conf.sample_rate, mc->uac->conf.channels);

    dc->fd = mc->on_read_fd;//use pipe fd to trigger event
    dc->priv = mc;
    if (write(mc->on_write_fd, &notify, 1) != 1) {
        loge("Failed writing to notify pipe: %s\n", strerror(errno));
        return -1;
    }
    return 0;

failed:
    if (mc->name) {
        free(mc->name);
    }
    if (mc) {
        free(mc);
    }
    if (fds[0] != -1 || fds[1] != -1) {
        close(fds[0]);
        close(fds[1]);
    }
    return -1;
}

static int mic_read(struct device_ctx *dc, void *buf, int len)
{
#if 0
    struct mic_ctx *mc = (struct mic_ctx *)dc->priv;
    char notify;
    struct video_frame *frm = buf;

    if (read(mc->on_read_fd, &notify, sizeof(notify)) != 1) {
        perror("Failed read from notify pipe");
    }

    return uac_query_frame(mc->uac, frm);
#else
    return 0;
#endif
}

static int mic_query(struct device_ctx *dc, struct media_frame *frame)
{
#if 0
    struct mic_ctx *mc = (struct mic_ctx *)dc->priv;
    char notify;

    if (read(mc->on_read_fd, &notify, sizeof(notify)) != 1) {
        perror("Failed read from notify pipe");
    }

    return uac_query_frame(mc->uac, &frame->video);
#else
    return 0;
#endif
}

static int mic_write(struct device_ctx *dc, void *buf, int len)
{
#if 0
    struct mic_ctx *mc = (struct mic_ctx *)dc->priv;
    char notify = '1';
    if (write(mc->on_write_fd, &notify, 1) != 1) {
        loge("Failed writing to notify pipe: %s\n", strerror(errno));
        return -1;
    }
    return 0;
#else
    return 0;
#endif
}

static void mic_close(struct device_ctx *dc)
{
#if 0
    struct mic_ctx *mc = (struct mic_ctx *)dc->priv;
    uac_close(mc->uac);
    free(mc->name);
    close(mc->on_read_fd);
    close(mc->on_write_fd);
    free(mc);
#endif
}

static int mic_ioctl(struct device_ctx *dc, uint32_t cmd, void *buf, int len)
{
    return 0;
}


struct device aq_mic_device = {
    .name = "microphone",
    .open = mic_open,
    .read = mic_read,
    .query_frame = mic_query,
    .write = mic_write,
    .ioctl = mic_ioctl,
    .close = mic_close,
};
