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
#ifndef _PLAYBACK_H_
#define _PLAYBACK_H_

#include <stdint.h>
#include <stdlib.h>
#include "common.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

struct playback_ctx {
    struct url url;
    struct playback *ops;
    struct media_encoder ma;
    void *priv;
};

struct playback {
    const char *name;
    int (*open)(struct playback_ctx *c, const char *type, struct media_encoder *format);
    int (*read)(struct playback_ctx *c, void *buf, int len);
    int (*write)(struct playback_ctx *c, void *buf, int len);
    void (*close)(struct playback_ctx *c);
    struct playback *next;
};

struct playback_ctx *playback_open(const char *url, struct media_encoder *format);
int playback_read(struct playback_ctx *c, void *buf, int len);
int playback_write(struct playback_ctx *c, void *buf, int len);
void playback_close(struct playback_ctx *c);

void playback_register_all();

#ifdef __cplusplus
}
#endif
#endif
