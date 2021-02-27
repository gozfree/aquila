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
#include <sys/uio.h>
#include <gear-lib/libsock.h>
#include <gear-lib/liblog.h>
#include <gear-lib/libgevent.h>
#include <gear-lib/libvector.h>
#include "protocol.h"
#include "common.h"
#include "rtp_h264.h"


/* RTP Header
 * |7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|7 6 5 4 3 2 1 0|
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |V=2|P|X|   CC  |M|      PT     |         sequence number       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           timestamp                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |            synchronization source (SSRC) identifier           |
 * +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 */

#define MAX_ADDR_STRING (65)

struct rtp_ctx {
    int rtp_fd;
    int rtcp_fd;
    uint16_t src_rtp_port;
    uint16_t src_rtcp_port;
    char src_ip[MAX_ADDR_STRING];
    char dst_ip[MAX_ADDR_STRING];
    uint16_t dst_port;
    struct gevent_base *evbase;
};

typedef struct rtp_packet {
    struct iovec *iobuf;

} rtp_packet_t;

typedef struct rtcp_packet {
    struct iovec *iobuf;

} rtcp_packet_t;

typedef struct rtp_header {
    uint8_t csrccount   :4;
    uint8_t extension   :1;
    uint8_t padding     :1;
    uint8_t version     :2;

    uint8_t payloadtype :7;
    uint8_t marker      :1;
    uint16_t sequencenumber;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_header_t;

enum rtp_payload_type {
    RTP_PAYLOAD_TYPE_PCMU    = 0,
    RTP_PAYLOAD_TYPE_PCMA    = 8,
    RTP_PAYLOAD_TYPE_JPEG    = 26,
    RTP_PAYLOAD_TYPE_H264    = 96,
    RTP_PAYLOAD_TYPE_H265    = 97,
    RTP_PAYLOAD_TYPE_OPUS    = 98,
    RTP_PAYLOAD_TYPE_AAC     = 99,
    RTP_PAYLOAD_TYPE_G726    = 100,
    RTP_PAYLOAD_TYPE_G726_16 = 101,
    RTP_PAYLOAD_TYPE_G726_24 = 102,
    RTP_PAYLOAD_TYPE_G726_32 = 103,
    RTP_PAYLOAD_TYPE_G726_40 = 104,
    RTP_PAYLOAD_TYPE_SPEEX   = 105,
};

#define RTP_PORT     (5560)
#define RTCP_PORT    (RTP_PORT+1)

#define RTP_PACKET_BUF_LEN_MAX	(1024)


static void on_recv_rtp(int fd, void *arg)
{
    int rlen;
    //struct rtp_ctx *c = (struct rtp_ctx *)arg;
    struct rtp_packet rtp_pkt;
    rtp_pkt.iobuf = iovec_create(RTP_PACKET_BUF_LEN_MAX);
    rlen = sock_recv(fd, rtp_pkt.iobuf->iov_base, RTP_PACKET_BUF_LEN_MAX);
    if (rlen > 0) {
        logi("sock_recv len = %d\n", rlen);
    } else if (rlen == 0) {
        loge("peer connect shutdown\n");
    } else {
        loge("something error\n");
    }
}

static void on_recv_rtcp(int fd, void *arg)
{
    int rlen;
    struct rtp_packet rtcp_pkt;
    rtcp_pkt.iobuf = iovec_create(RTP_PACKET_BUF_LEN_MAX);
    rlen = sock_recv(fd, rtcp_pkt.iobuf->iov_base, RTP_PACKET_BUF_LEN_MAX);
    if (rlen > 0) {
        logi("sock_recv len = %d\n", rlen);
    } else if (rlen == 0) {
        loge("peer connect shutdown\n");
    } else {
        loge("something error\n");
    }
}

static void on_error(int fd, void *arg)
{
    loge("error: %d\n", errno);
}


static int rtp_open(struct protocol_ctx *sc, const char *url, struct media_encoder *media, void *conf)
{
    struct rtp_ctx *c = (struct rtp_ctx *)sc->priv;
    sock_addr_list_t *tmp;
    char str[MAX_ADDR_STRING];
    char ip[64];
    int len;
    char *p;
    const char *tag = ":";
    logi("url = %s\n", url);
    p = strstr((char *)url, tag);
    if (!p) {
        loge("rtp url is invalid\n");
        return -1;
    }
    len = p - url;
    strncpy(c->dst_ip, url, len);
    p += strlen(tag);
    c->dst_port = atoi(p);

    memset(str, 0, MAX_ADDR_STRING);
    if (0 == sock_get_local_list(&tmp, 0)) {
        for (; tmp; tmp = tmp->next) {
            sock_addr_ntop(str, tmp->addr.ip);
            logd("ip = %s port = %d\n", str, tmp->addr.port);
        }
    }
    strncpy(c->src_ip, str, MAX_ADDR_STRING);

    c->src_rtp_port = RTP_PORT;
    c->rtp_fd = sock_udp_bind(c->src_ip, RTP_PORT);//random port
    if (c->rtp_fd == -1) {
        loge("bind %s failed\n", ip);
        return -1;
    }
    sock_set_noblk(c->rtp_fd, 1);
    sock_set_reuse(c->rtp_fd, 1);

    c->src_rtcp_port = RTCP_PORT;
    c->rtcp_fd = sock_udp_bind(c->src_ip, RTCP_PORT);//random port
    if (c->rtcp_fd == -1) {
        loge("bind %s failed\n", ip);
        return -1;
    }
    sock_set_noblk(c->rtcp_fd, 1);
    sock_set_reuse(c->rtcp_fd, 1);
    c->evbase = gevent_base_create();
    if (!c->evbase) {
        return -1;
    }
    struct gevent *ev_rtp = gevent_create(c->rtp_fd, on_recv_rtp, NULL, on_error, (void *)c);
    if (-1 == gevent_add(c->evbase, &ev_rtp)) {
        loge("event_add failed!\n");
        gevent_destroy(ev_rtp);
    }
    struct gevent *ev_rtcp = gevent_create(c->rtcp_fd, on_recv_rtcp, NULL, on_error, (void *)c);
    if (-1 == gevent_add(c->evbase, &ev_rtcp)) {
        loge("event_add failed!\n");
        gevent_destroy(ev_rtcp);
    }

    logi("url: \"rtp://%s:%d\"\n", c->src_ip, c->dst_port);
    return 0;
}

static int rtp_read(struct protocol_ctx *sc, void *buf, int len)
{
    return 0;
}

static int rtp_packet_assemble(int32_t pts, void *buf, size_t len)
{
    uint8_t *rtp_header = (uint8_t *)buf;
    static uint16_t sequence_num = 0;
    static uint32_t time_stamp = 0;
    ++sequence_num;
    time_stamp += pts;
    rtp_header[0] = 0x80;
    rtp_header[1] = RTP_PAYLOAD_TYPE_H264;
    rtp_header[2] = (sequence_num & 0xff00) >> 8;
    rtp_header[3] = sequence_num & 0x00ff;
    rtp_header[4] = (time_stamp & 0xff000000) >> 24;
    rtp_header[5] = (time_stamp & 0x00ff0000) >> 16;
    rtp_header[6] = (time_stamp & 0x0000ff00) >> 8;
    rtp_header[7] = (time_stamp & 0x000000ff);
    rtp_header[8] = 0; // SSRC[3]
    rtp_header[9] = 0; // SSRC[2]
    rtp_header[10] = 0; // SSRC[1]
    rtp_header[11] = 0; // SSRC[0]
    return 0;
}

static int rtp_packet_send(struct rtp_ctx *c, void *buf, size_t len)
{
    uint8_t *data = (uint8_t *)buf;
    h264_find_nalu(buf, len);
    rtp_packet_assemble(0, buf, len);
#if 1
    uint8_t *ssrc = &data[8];
    uint32_t SSRC = (uint32_t)-1;
    ssrc[0] = (SSRC & 0xff000000) >> 24;
    ssrc[1] = (SSRC & 0x00ff0000) >> 16;
    ssrc[2] = (SSRC & 0x0000ff00) >>  8;
    ssrc[3] = (SSRC & 0x000000ff);
#endif
    sock_sendto(c->rtp_fd, c->dst_ip, c->dst_port, data, len);
    //print_buffer(data, len);
    return 0;
}

static int rtp_write(struct protocol_ctx *sc, void *buf, int len)
{
    struct rtp_ctx *c = (struct rtp_ctx *)sc->priv;
    int ret = rtp_packet_send(c, buf, len);
    logi("sock_sendto len = %d\n", ret);
    return ret;
}

static void rtp_close(struct protocol_ctx *sc)
{
    struct rtp_ctx *c = (struct rtp_ctx *)sc->priv;
    sock_close(c->rtp_fd);
    sock_close(c->rtcp_fd);
}

struct protocol aq_rtp_protocol = {
    .name = "rtp",
    .open = rtp_open,
    .read = rtp_read,
    .write = rtp_write,
    .close = rtp_close,
};
