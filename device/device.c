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
#include <string.h>
#include <libmacro.h>
#include <libatomic.h>
#include <liblog.h>
#include "device.h"

#define REGISTER_DEVICE(x)                                                     \
    {                                                                          \
        extern struct device aq_##x##_device;                                  \
            device_register(&aq_##x##_device);                                 \
    }

static struct device *first_device = NULL;
static struct device **last_device = &first_device;
static int registered = 0;

static void device_register(struct device *dev)
{
    struct device **p = last_device;
    dev->next = NULL;
    while (*p || atomic_ptr_cas((void * volatile *)p, NULL, dev))
        p = &(*p)->next;
    last_device = &dev->next;
}


void device_register_all(void)
{
    if (registered)
        return;
    registered = 1;

    REGISTER_DEVICE(v4l2);
    REGISTER_DEVICE(vdevfake);
}

struct device_ctx *device_open(const char *url, struct media_params *media)
{
    struct device *p;
    struct device_ctx *dc = CALLOC(1, struct device_ctx);
    if (!dc) {
        loge("malloc device context failed!\n");
        return NULL;
    }
    if (-1 == url_parse(&dc->url, url)) {
        loge("url_parse failed!\n");
        return NULL;
    }

    for (p = first_device; p != NULL; p = p->next) {
        if (!strcasecmp(dc->url.head, p->name))
            break;
    }
    if (p == NULL) {
        loge("%s device does not support!\n", dc->url.head);
        goto failed;
    }
    logi("\t[device module] <%s> %s loaded\n", p->name, dc->url.body);

    dc->ops = p;

    if (!dc->ops->open) {
        loge("device open ops can't be null\n");
        goto failed;
    }
    if (0 != dc->ops->open(dc, dc->url.body, media)) {
        loge("open %s failed!\n", dc->url.body);
        goto failed;
    }
    return dc;
failed:
    if (dc) {
        free(dc);
    }
    return NULL;
}

int device_read(struct device_ctx *dc, void *buf, int len)
{
    if (!dc->ops->read)
        return -1;
    return dc->ops->read(dc, buf, len);
}

int device_write(struct device_ctx *dc, void *buf, int len)
{
    if (!dc->ops->write)
        return -1;
    return dc->ops->write(dc, buf, len);
}

int device_ioctl(struct device_ctx *dc, uint32_t cmd, void *buf, int len)
{
    if (!dc->ops->ioctl)
        return -1;
    return dc->ops->ioctl(dc, cmd, buf, len);
}

void device_close(struct device_ctx *dc)
{
    if (!dc) {
        return;
    }
    if (dc->ops->close) {
        dc->ops->close(dc);
    }
    free(dc);
}

