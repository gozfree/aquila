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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <gear-lib/liblog.h>
#include <gear-lib/librtsp.h>
#include <gear-lib/libmacro.h>
#include <gear-lib/libqueue.h>
#include "protocol.h"
#include "common.h"


struct proxy_source_ctx {
    const char name[32];
    struct iovec data;
    struct uvc_ctx *uvc;
    bool is_opened;
    int width;
    int height;
    struct iovec extradata;
    struct queue *q;
};

static struct proxy_source_ctx g_proxy;

static void *item_alloc_hook(void *data, size_t len, void *arg)
{
    struct media_packet *pkt = (struct media_packet *)arg;
    if (!pkt) {
        loge("calloc packet failed!\n");
        return NULL;
    }
    struct media_packet *new_pkt = media_packet_copy(pkt);
    logd("media_packet size=%d\n", media_packet_get_size(new_pkt));
    return new_pkt;
}

static void item_free_hook(void *data)
{
    struct media_packet *pkt = (struct media_packet *)data;
    media_packet_destroy(pkt);
}

static int proxy_open(struct media_source *ms, const char *name)
{
    struct proxy_source_ctx *c = &g_proxy;
    c->q = queue_create();
    if (!c->q) {
        loge("queue_create failed!\n");
        return -1;
    }
    queue_set_hook(c->q, item_alloc_hook, item_free_hook);
    ms->opaque = c;

    return 0;
}

static void proxy_close(struct media_source *ms)
{
    struct proxy_source_ctx *c = (struct proxy_source_ctx *)ms->opaque;
    queue_destroy(c->q);
}

static int is_auth()
{
    return 0;
}

static uint32_t get_random_number()
{
    struct timeval now = {0};
    gettimeofday(&now, NULL);
    srand(now.tv_usec);
    return (rand() % ((uint32_t)-1));
}


static int sdp_generate(struct media_source *ms)
{
    int n = 0;
    char p[SDP_LEN_MAX];
    uint32_t session_id = get_random_number();
    gettimeofday(&ms->tm_create, NULL);
    n += snprintf(p+n, sizeof(p)-n, "v=0\n");
    n += snprintf(p+n, sizeof(p)-n, "o=%s %"PRIu32" %"PRIu32" IN IP4 %s\n", is_auth()?"username":"-", session_id, 1, "0.0.0.0");
    n += snprintf(p+n, sizeof(p)-n, "s=%s\n", ms->name);
    n += snprintf(p+n, sizeof(p)-n, "i=%s\n", ms->info);
    n += snprintf(p+n, sizeof(p)-n, "c=IN IP4 0.0.0.0\n");
    n += snprintf(p+n, sizeof(p)-n, "t=0 0\n");
    n += snprintf(p+n, sizeof(p)-n, "a=range:npt=0-\n");
    n += snprintf(p+n, sizeof(p)-n, "a=sendonly\n");
    n += snprintf(p+n, sizeof(p)-n, "a=control:*\n");
    n += snprintf(p+n, sizeof(p)-n, "a=source-filter: incl IN IP4 * %s\r\n", "0.0.0.0");
    n += snprintf(p+n, sizeof(p)-n, "a=rtcp-unicast: reflection\r\n");
    n += snprintf(p+n, sizeof(p)-n, "a=x-qt-text-nam:%s\r\n", ms->name);
    n += snprintf(p+n, sizeof(p)-n, "a=x-qt-text-inf:%s\r\n", ms->info);

    n += snprintf(p+n, sizeof(p)-n, "m=video 0 RTP/AVP 96\r\n");
    n += snprintf(p+n, sizeof(p)-n, "a=rtpmap:96 H264/90000\r\n");
    n += snprintf(p+n, sizeof(p)-n, "a=fmtp:96 packetization-mode=1; profile-level-id=4D4028; sprop-parameter-sets=Z01AKJpkA8ARPy4C3AQEBQAAAwPoAADqYOhgBGMAAF9eC7y40MAIxgAAvrwXeXCg,aO44gA==;\r\n");
    n += snprintf(p+n, sizeof(p)-n, "a=cliprect:0,0,240,320\r\n");
    strcpy(ms->sdp, p);
    return 0;
}

static int proxy_read(struct media_source *ms, void **data, size_t *len)
{
    struct proxy_source_ctx *c = (struct proxy_source_ctx *)ms->opaque;
    struct item *it = queue_pop(c->q);
    *data = (struct media_packet *)it->opaque.iov_base;
    *len = it->data.iov_len;
    logd("queue_pop ptr=%p, data=%p, len=%d\n", it->opaque.iov_base, *data, it->opaque.iov_len);
    //item_free(c->q, it);
    return 0;
}

struct media_source media_source_proxy = {
    .name         = "proxy",
    .sdp_generate = sdp_generate,
    .open         = proxy_open,
    .read         = proxy_read,
    .close        = proxy_close,
};

extern struct media_source media_source_proxy;

static int rtsp_open(struct protocol_ctx *pc, const char *url, struct media_encoder *media)
{
    struct rtsp_server *rc = rtsp_server_init(NULL, 8554);
    if (!rc) {
        loge("rtsp_server_init failed!\n");
        return -1;
    }
    logd("url=%s\n", url);
    rtsp_media_source_register(&media_source_proxy);
    logi("rtsp_media_source_register %s:%p\n", media_source_proxy.name, &media_source_proxy);
    rtsp_server_dispatch(rc);

    pc->priv = rc;
    return 0;
}

static int rtsp_read(struct protocol_ctx *pc, void *buf, int len)
{
    loge("xxxx\n");
    return 0;
}

static int rtsp_write(struct protocol_ctx *pc, void *buf, int len)
{
    struct media_source *ms = rtsp_media_source_lookup("proxy");
    if (!ms) {
        loge("rtsp_media_source_lookup proxy failed!\n");
        return -1;
    }
    if (!rtsp_media_source_alive(ms)) {
        logd("rtsp_media_source %s is not alive\n", ms->name);
        return 0;
    }
    struct proxy_source_ctx *c = (struct proxy_source_ctx *)ms->opaque;
    struct media_packet *pkt = buf;
    struct item *it = NULL;
    switch (pkt->type) {
    case MEDIA_TYPE_AUDIO:
        it = item_alloc(c->q, pkt->audio->data, pkt->audio->size, pkt);
        break;
    case MEDIA_TYPE_VIDEO:
        it = item_alloc(c->q, pkt->video->data, pkt->video->size, pkt);
        break;
    default:
        break;
    }
    if (!it) {
        loge("item_alloc packet type %d failed!\n", pkt->type);
        return -1;
    }
    if (0 != queue_push(c->q, it)) {
        loge("queue_push failed!\n");
        return -1;
    }

    return 0;
}

static void rtsp_close(struct protocol_ctx *pc)
{
    struct rtsp_server *rc = (struct rtsp_server *)pc->priv;
    rtsp_server_deinit(rc);
}

struct protocol aq_rtsp_protocol = {
    .name = "rtsp",
    .open = rtsp_open,
    .read = rtsp_read,
    .write = rtsp_write,
    .close = rtsp_close,
};
