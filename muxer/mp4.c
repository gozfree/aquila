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
#include <gear-lib/liblog.h>
#include <gear-lib/libmp4.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include "muxer.h"
#include "common.h"


static int mp4_open(struct muxer_ctx *mc, struct media_encoder *me)
{
    struct mp4_config conf = {
        .width = me->video.width,
        .height = me->video.height,
        .fps.num = me->video.framerate.num,
        .fps.den = me->video.framerate.den,
        .format = me->video.format,
    };
    struct mp4_muxer *mp4 = mp4_muxer_open(mc->url.body, &conf);

    mc->priv = mp4;
    return 0;
}

static int mp4_write(struct muxer_ctx *mc, struct media_packet *pkt)
{
    struct mp4_muxer *mp4 = (struct mp4_muxer *)mc->priv;
    mp4_muxer_write(mp4, pkt);

    return 0;
}

void mp4_close(struct muxer_ctx *mc)
{
    struct mp4_muxer *mp4 = (struct mp4_muxer *)mc->priv;
    mp4_muxer_close(mp4);
}

struct muxer aq_mp4_muxer = {
    .name         = "mp4",
    .open         = mp4_open,
    .write_header = NULL,
    .write_packet = mp4_write,
    .write_tailer = NULL,
    .read_header  = NULL,
    .read_packet  = NULL,
    .read_tailer  = NULL,
    .close        = mp4_close,
};
