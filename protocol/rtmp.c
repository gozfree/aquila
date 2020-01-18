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
#include <unistd.h>
#include <string.h>

#include <libmacro.h>
#include <liblog.h>
#include <libtime.h>
#include <librtmp.h>

#include "protocol.h"

enum rtmp_status {
    RTMP_STATUS_IDLE,
    RTMP_STATUS_RUNNING,
    RTMP_STATUS_STOPPED,
};

typedef struct rtmp_ctx {
    enum rtmp_status status;
    struct media_params mp;
    char *url;
    struct rtmp *client;
} rtmp_ctx_t;

static int rtmp_open(struct protocol_ctx *pc, const char *url, struct media_params *mp)
{
    struct rtmp_ctx *rc = CALLOC(1, struct rtmp_ctx);
    if (!rc) {
        loge("malloc rtmp_ctx failed!\n");
        goto failed;
    }
    rc->status = RTMP_STATUS_IDLE;
    rc->client = rtmp_create(url);
    if(!rc->client) {
        loge("failed to connect %s, please make sure rtmp server exist\n", url);
        goto failed;
    }
    logi("url = %s\n", url);
    rc->url = strdup(url);
    memcpy(&rc->mp, mp, sizeof(struct media_params));
    pc->priv = rc;
    return 0;

failed:
    if (rc) {
        free(rc);
    }
    return -1;
}

static int rtmp_write(struct protocol_ctx *pc, void *buf, int len)
{
    struct rtmp_ctx *rc = (struct rtmp_ctx *)pc->priv;
    int ret = 0;
    struct media_packet *pkt = buf;

    if (rc->status == RTMP_STATUS_IDLE) {
        if (rtmp_stream_add(rc->client, pkt)) {
            loge("rtmp_stream_add failed!\n");
            ret = 0;
        } else {
            rtmp_stream_start(rc->client);
            rc->status = RTMP_STATUS_RUNNING;
            logi("get_extra_data success!\n");
        }
    } else if (rc->status == RTMP_STATUS_RUNNING) {
        rtmp_send_packet(rc->client, pkt);
    }
    return ret;
}

static void rtmp_close(struct protocol_ctx *pc)
{
    struct rtmp_ctx *rc = (struct rtmp_ctx *)pc->priv;
    rtmp_stream_stop(rc->client);
    free(rc->url);
    free(rc);
}

struct protocol aq_rtmp_protocol = {
    .name = "rtmp",
    .open = rtmp_open,
    .read = NULL,
    .write = rtmp_write,
    .close = rtmp_close,
};
