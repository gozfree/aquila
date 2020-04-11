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
#ifndef _MUXER_H_
#define _MUXER_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include "common.h"
#include "url.h"

#ifdef __cplusplus
extern "C" {
#endif

struct muxer_ctx {
    struct url url;
    struct muxer *ops;
    void *priv;
};

struct muxer {
    const char *name;
    int (*open)(struct muxer_ctx *c, struct media_encoder *ma);
    int (*write_header)(struct muxer_ctx *c);
    int (*write_packet)(struct muxer_ctx *c, struct media_packet *pkt);
    int (*write_tailer)(struct muxer_ctx *c);


    int (*read_header)(struct muxer_ctx *c);
    int (*read_packet)(struct muxer_ctx *c, struct media_packet *pkt);
    int (*read_tailer)(struct muxer_ctx *c);

    void (*close)(struct muxer_ctx *c);
    struct muxer *next;
};

void muxer_register_all();
struct muxer_ctx *muxer_open(const char *name, struct media_encoder *ma);
void muxer_close(struct muxer_ctx *c);

int muxer_write_header(struct muxer_ctx *c);
int muxer_write_packet(struct muxer_ctx *c, struct media_packet *pkt);
int muxer_write_tailer(struct muxer_ctx *c);

int muxer_read_header(struct muxer_ctx *c);
int muxer_read_packet(struct muxer_ctx *c, struct media_packet *pkt);
int muxer_read_header(struct muxer_ctx *c);


#ifdef __cplusplus
}
#endif
#endif
