/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    device.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-04-21 22:23
 * updated: 2016-04-21 22:23
 ******************************************************************************/
#ifndef _DEVICE_H_
#define _DEVICE_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>

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


struct device_ctx {
    int fd;
    struct url url;
    struct device *ops;
    struct media_params media;
    void *priv;
};

struct device {
    const char *name;
    int (*open)(struct device_ctx *c, const char *url, struct media_params *media);
    int (*read)(struct device_ctx *c, void *buf, int len);
    int (*write)(struct device_ctx *c, void *buf, int len);
    int (*ioctl)(struct device_ctx *c, uint32_t cmd, void *buf, int len);
    void (*close)(struct device_ctx *c);
    struct device *next;
};


void device_register_all();
struct device_ctx *device_open(const char *url, struct media_params *media);
int device_read(struct device_ctx *dc, void *buf, int len);
int device_write(struct device_ctx *dc, void *buf, int len);
int device_ioctl(struct device_ctx *dc, uint32_t cmd, void *buf, int len);
void device_close(struct device_ctx *dc);

#ifdef __cplusplus
}
#endif
#endif
