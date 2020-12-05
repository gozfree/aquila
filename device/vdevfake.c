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
#include <gear-lib/libuvc.h>
#include <gear-lib/libmedia-io.h>
#include <gear-lib/liblog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "device.h"

struct dummy_ctx {
    struct uvc_config conf;
    struct uvc_ctx *uvc;
    char *name;
    int on_read_fd;
    int on_write_fd;
};

static int _open(struct device_ctx *dc, struct media_producer *mp)
{
    int fds[2] = {0};
    char notify = '1';
    const char *dev = dc->url.body;
    struct dummy_ctx *vc = CALLOC(1, struct dummy_ctx);
    if (!vc) {
        loge("malloc dummy_ctx failed!\n");
        return -1;
    }
    if (pipe(fds)) {
        loge("create pipe failed(%d): %s\n", errno, strerror(errno));
        goto failed;
    }
    vc->on_read_fd = fds[0];
    vc->on_write_fd = fds[1];
    logd("pipe: rfd = %d, wfd = %d\n", vc->on_read_fd, vc->on_write_fd);

    vc->conf.width = mp->video.width;
    vc->conf.height = mp->video.height;
    vc->conf.fps.num = 5;
    vc->conf.fps.den = 1;
    vc->conf.format = PIXEL_FORMAT_YUY2;
    media_producer_dump_info(mp);

    vc->uvc = uvc_open(UVC_TYPE_DUMMY, dev, &vc->conf);
    if (uvc_start_stream(vc->uvc, NULL)) {
        loge("uvc start stream failed!\n");
        goto failed;
    }
    vc->name = strdup(dev);
    logi("open %s format:%s resolution:%d*%d @%d/%dfps\n",
          dev, pixel_format_to_string(vc->uvc->conf.format),
          vc->uvc->conf.width, vc->uvc->conf.height,
          vc->uvc->conf.fps.num, vc->uvc->conf.fps.den);

    mp->video.framerate.num = vc->uvc->conf.fps.num;
    mp->video.framerate.den = vc->uvc->conf.fps.den;
    mp->video.format = vc->uvc->conf.format;
    dc->fd = vc->on_read_fd;//use pipe fd to trigger event
    dc->priv = vc;
    if (write(vc->on_write_fd, &notify, 1) != 1) {
        loge("Failed writing to notify pipe: %s\n", strerror(errno));
        return -1;
    }
    return 0;

failed:
    if (vc->name) {
        free(vc->name);
    }
    if (vc) {
        free(vc);
    }
    if (fds[0] != -1 || fds[1] != -1) {
        close(fds[0]);
        close(fds[1]);
    }
    return -1;
}

static int _read(struct device_ctx *dc, void *buf, size_t len)
{
    struct dummy_ctx *vc = (struct dummy_ctx *)dc->priv;
    char notify;
    struct video_frame *frm = buf;

    if (read(vc->on_read_fd, &notify, sizeof(notify)) != 1) {
        perror("Failed read from notify pipe");
    }

    return uvc_query_frame(vc->uvc, frm);
}

static int _query(struct device_ctx *dc, struct media_frame *frame)
{
    struct dummy_ctx *vc = (struct dummy_ctx *)dc->priv;
    char notify;

    if (read(vc->on_read_fd, &notify, sizeof(notify)) != 1) {
        perror("Failed read from notify pipe");
    }

    return uvc_query_frame(vc->uvc, &frame->video);
}

static int _write(struct device_ctx *dc, const void *buf, size_t len)
{
    struct dummy_ctx *vc = (struct dummy_ctx *)dc->priv;
    char notify = '1';
    if (write(vc->on_write_fd, &notify, 1) != 1) {
        loge("Failed writing to notify pipe: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static void _close(struct device_ctx *dc)
{
    struct dummy_ctx *vc = (struct dummy_ctx *)dc->priv;
    uvc_close(vc->uvc);
    free(vc->name);
    close(vc->on_read_fd);
    close(vc->on_write_fd);
    free(vc);
}

struct device aq_file_device = {
    .name        = "file",
    .open        = _open,
    .read        = _read,
    .query_frame = _query,
    .write       = _write,
    .ioctl       = NULL,
    .close       = _close,
};
