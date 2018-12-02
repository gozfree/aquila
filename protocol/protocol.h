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
#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>
#include <stdlib.h>
#include "common.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

struct protocol_ctx {
    struct url url;
    struct protocol *ops;
    void *priv;
};

struct protocol {
    const char *name;
    int (*open)(struct protocol_ctx *c, const char *url, struct media_params *media);
    int (*read)(struct protocol_ctx *c, void *buf, int len);
    int (*write)(struct protocol_ctx *c, void *buf, int len);
    void (*close)(struct protocol_ctx *c);
    struct protocol *next;
};

void protocol_register_all();
struct protocol_ctx *protocol_new(const char *url);
void protocol_free(struct protocol_ctx *c);

struct protocol_ctx *protocol_open(const char *url, struct media_params *media);

int protocol_read(struct protocol_ctx *c, void *buf, int len);
int protocol_write(struct protocol_ctx *c, void *buf, int len);
void protocol_close(struct protocol_ctx *c);


#ifdef __cplusplus
}
#endif
#endif
