/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    rtmp.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-22 22:35
 * updated: 2016-05-22 22:35
 ******************************************************************************/
#include <string.h>

#include <librtmp/rtmp.h>
#include <libmacro.h>
#include <liblog.h>

#include "protocol.h"


typedef struct rtmp_ctx {
    RTMP rtmp;
    RTMPPacket packet;
    struct media_params media;
} rtmp_ctx_t;


static int rtmp_open(struct protocol_ctx *pc, const char *url, struct media_params *media)
{
    struct rtmp_ctx *rc = CALLOC(1, struct rtmp_ctx);
    if (!rc) {
        loge("malloc rtmp_ctx failed!\n");
        goto failed;
    }

    RTMP_Init(&rc->rtmp);
    int ret = RTMP_SetupURL(&rc->rtmp, (char *)url);

    if (!ret) {
        loge("RTMP_SetupURL failed\n");
        goto failed;
    }
    RTMP_EnableWrite(&rc->rtmp);

    ret = RTMP_Connect(&rc->rtmp, NULL);
    if (!ret) {
        loge("RTMP_Connect failed\n");
        goto failed;
    }
    ret = RTMP_ConnectStream(&rc->rtmp, 0);

    if (!ret) {
        RTMP_Close(&rc->rtmp);
        loge("RTMP_ConnectStream failed\n");
        goto failed;
    }
    memcpy(&rc->media, media, sizeof(struct media_params));
    pc->priv = rc;
    return 0;

failed:
    if (rc) {
        free(rc);
    }
    return -1;
}

static int rtmp_read(struct protocol_ctx *pc, void *buf, int len)
{
    struct rtmp_ctx *rc = (struct rtmp_ctx *)pc->priv;
    return RTMP_Read(&rc->rtmp, buf, len);
}

static int rtmp_write(struct protocol_ctx *pc, void *buf, int len)
{
    struct rtmp_ctx *rc = (struct rtmp_ctx *)pc->priv;
    int ret = 0;
    RTMPPacket *packet = &rc->packet;

    RTMPPacket_Alloc(packet, len);
    RTMPPacket_Reset(packet);
    if (rc->media.type == RTMP_PACKET_TYPE_INFO) { // metadata
        packet->m_nChannel = 0x03;
    } else if (rc->media.type == RTMP_PACKET_TYPE_VIDEO) { // video
        packet->m_nChannel = 0x04;
    } else if (rc->media.type == RTMP_PACKET_TYPE_AUDIO) { //audio
        packet->m_nChannel = 0x05;
    } else {
        packet->m_nChannel = -1;
    }

    packet->m_nInfoField2  =  rc->rtmp.m_stream_id;

    memcpy(packet->m_body, buf, len);
    packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
    packet->m_hasAbsTimestamp = FALSE;
    packet->m_nTimeStamp = rc->media.video.timestamp;
    packet->m_packetType = rc->media.type;
    packet->m_nBodySize  = len;
    ret = RTMP_SendPacket(&rc->rtmp, &rc->packet, 0);

    RTMPPacket_Free(packet);
    if (!ret) {
        loge("RTMP_SendPacket failed\n");
    }
    return ret;
}

static void rtmp_close(struct protocol_ctx *pc)
{
    struct rtmp_ctx *rc = (struct rtmp_ctx *)pc->priv;
    RTMP_Close(&rc->rtmp);
    free(rc);
}

struct protocol aq_rtmp_protocol = {
    .name = "rtmp",
    .open = rtmp_open,
    .read = rtmp_read,
    .write = rtmp_write,
    .close = rtmp_close,
};
