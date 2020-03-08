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
#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <gear-lib/libmedia-io.h>

#include "common.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOTE:
 * in linux kernel
 * platform_driver is contain ops, like device in here
 * platform_device is actually context, like device_ctx in here.
 * and context alloc inner in "probe", free in "remove",
 * because probe/remove is triggerd by kernel.
 * here context alloc outside in the device class.
*/

enum device_type {
    DEVICE_VIDEO_DECODE,   //v4l2 output raw/yuv/rgb
    DEVICE_VIDEO_ENCODE,   //his/amba h264 stream
    DEVICE_AUDIO_DECODE,   //pulse audio
    DEVICE_AUDIO_ENCODE,   //his/amba aac stream
};

struct device_buffer {
    union {
        struct media_frame *frame;
        struct media_packet *packet;
    };
};


struct device_ctx {
    int fd;
    struct url url;
    enum device_type type;
    struct device *ops;
    //struct media_attr media;
    void *priv;
};

struct device {
    const char *name;
    int (*open)(struct device_ctx *c, const char *url, struct media_attr *ma);
    int (*read)(struct device_ctx *c, void *buf, int len);
    int (*query_frame)(struct device_ctx *c, struct media_frame *frame);
    int (*query_packet)(struct device_ctx *c, struct media_packet *packet);
    int (*write)(struct device_ctx *c, void *buf, int len);
    int (*ioctl)(struct device_ctx *c, uint32_t cmd, void *buf, int len);
    void (*close)(struct device_ctx *c);
    struct device *next;
};


void device_register_all();
struct device_ctx *device_open(const char *url, struct media_attr *ma);
int device_read(struct device_ctx *dc, void *buf, int len);
int device_query_buffer(struct device_ctx *dc, struct device_buffer *buf);
int device_write(struct device_ctx *dc, void *buf, int len);
int device_ioctl(struct device_ctx *dc, uint32_t cmd, void *buf, int len);
void device_close(struct device_ctx *dc);
struct device_ctx *device_get();

#ifdef __cplusplus
}
#endif
#endif
