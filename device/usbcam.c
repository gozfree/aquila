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
#include <libuvc.h>
#include <libmacro.h>
#include <liblog.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include "device.h"
#include "common.h"

struct usbcam_ctx {
    struct uvc_ctx *uvc;
    char *name;
    int on_read_fd;
    int on_write_fd;
};

static int usbcam_open(struct device_ctx *dc, const char *dev, struct media_params *media)
{
    int fds[2] = {0};
    char notify = '1';
    struct usbcam_ctx *vc = CALLOC(1, struct usbcam_ctx);
    if (!vc) {
        loge("malloc usbcam_ctx failed!\n");
        return -1;
    }
    if (pipe(fds)) {
        loge("create pipe failed(%d): %s\n", errno, strerror(errno));
        goto failed;
    }
    vc->on_read_fd = fds[0];
    vc->on_write_fd = fds[1];
    logd("pipe: rfd = %d, wfd = %d\n", vc->on_read_fd, vc->on_write_fd);

    vc->uvc = uvc_open(dev, media->video.width, media->video.height);

    vc->name = strdup(dev);
    if (uvc_print_info(vc->uvc) < 0) {
        loge("uvc_print_info failed!\n");
        goto failed;
    }

    dc->fd = vc->on_read_fd;//use pipe fd to trigger event
    dc->media.video.width = media->video.width;
    dc->media.video.height = media->video.height;
    dc->media.video.pix_fmt = YUV422P;
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

static int usbcam_read(struct device_ctx *dc, void *buf, int len)
{
    struct usbcam_ctx *vc = (struct usbcam_ctx *)dc->priv;
    char notify;
    int ret;

    if (read(vc->on_read_fd, &notify, sizeof(notify)) != 1) {
        perror("Failed read from notify pipe");
    }

    ret = uvc_read(vc->uvc, buf, len);
    return ret;
}

static int usbcam_write(struct device_ctx *dc, void *buf, int len)
{
    struct usbcam_ctx *vc = (struct usbcam_ctx *)dc->priv;
    char notify = '1';
    if (write(vc->on_write_fd, &notify, 1) != 1) {
        loge("Failed writing to notify pipe: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

static void usbcam_close(struct device_ctx *dc)
{
    struct usbcam_ctx *vc = (struct usbcam_ctx *)dc->priv;
    uvc_close(vc->uvc);
    free(vc->name);
    close(vc->on_read_fd);
    close(vc->on_write_fd);
    free(vc);
}

static int usbcam_ioctl(struct device_ctx *dc, uint32_t cmd, void *buf, int len)
{
    struct usbcam_ctx *vc = (struct usbcam_ctx *)dc->priv;
    struct video_ctrl *vctrl;
    switch (cmd) {
#if 0
    case DEV_VIDEO_GET_CAP:
        //usbcam_get_cap(vc);
        break;
    case DEV_VIDEO_SET_CTRL:
        vctrl = (struct video_ctrl *)buf;
        usbcam_set_control(vc, vctrl->cmd, vctrl->val);
        break;
#endif
    default:
        uvc_print_info(vc->uvc);
        break;
    }
    return 0;
}

struct device aq_usbcam_device = {
    "usbcam",
    usbcam_open,
    usbcam_read,
    usbcam_write,
    usbcam_ioctl,
    usbcam_close,
};
