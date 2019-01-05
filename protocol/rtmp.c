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

#include "protocol.h"
#ifdef USE_RTMPCLIENT
#include "rtmpclient.h"
#else
#include "librtmp.h"
#endif

/* NAL unit types */
enum {
    H264_NAL_SLICE           = 1,
    H264_NAL_DPA             = 2,
    H264_NAL_DPB             = 3,
    H264_NAL_DPC             = 4,
    H264_NAL_IDR_SLICE       = 5,
    H264_NAL_SEI             = 6,
    H264_NAL_SPS             = 7,
    H264_NAL_PPS             = 8,
    H264_NAL_AUD             = 9,
    H264_NAL_END_SEQUENCE    = 10,
    H264_NAL_END_STREAM      = 11,
    H264_NAL_FILLER_DATA     = 12,
    H264_NAL_SPS_EXT         = 13,
    H264_NAL_AUXILIARY_SLICE = 19,
};

#define NAL_HEADER_LEN     4
#define NAL_HEADER_PREFIX(data) \
        ((data[0] == 0x00) &&   \
         (data[1] == 0x00) &&   \
         (data[2] == 0x00) &&   \
         (data[3] == 0x01))

#define NAL_SLICE_PREFIX(data) \
        ((data[0] == 0x00) &&  \
         (data[1] == 0x00) &&  \
         (data[2] == 0x01))

#define NAL_TYPE(data) \
        (data[4]&0x1F)

enum rtmp_status {
    RTMP_STATUS_IDLE,
    RTMP_STATUS_RUNNING,
    RTMP_STATUS_STOPPED,
};

typedef struct media_param {
    int aac_bitrate;
    int aac_samplerate;
    int framerate;
    int width;
    int height;
} media_param_t;


typedef struct rtmp_ctx {
    enum rtmp_status status;
    struct media_params media;
    char *url;
#ifdef USE_RTMPCLIENT
    RtmpClient *client;
#else
    struct rtmp *client;
#endif
} rtmp_ctx_t;

static int rtmp_open(struct protocol_ctx *pc, const char *url, struct media_params *media)
{
    struct rtmp_ctx *rc = CALLOC(1, struct rtmp_ctx);
    if (!rc) {
        loge("malloc rtmp_ctx failed!\n");
        goto failed;
    }
    rc->status = RTMP_STATUS_IDLE;
#ifdef USE_RTMPCLIENT
    rc->client = new RtmpClient;
#else
    rc->client = rtmp_create(url);
#endif
    if(!rc->client) {
        loge("failed to new RtmpClient\n");
    }
    logi("url = %s\n", url);
    rc->url = strdup(url);
    memcpy(&rc->media, media, sizeof(struct media_params));
    pc->priv = rc;
    return 0;

failed:
    if (rc) {
        free(rc);
    }
    return -1;
}

static int strip_nalu_sps_pps(uint8_t *data, int len)
{
    int got_sps = 0, got_pps = 0;
    int pos = 0;
    int i = 0, j = 0;
    int index[16] = {0};
    while (pos < len - 4) {
        if (NAL_SLICE_PREFIX((data+pos))) {
            index[i] = pos;
            ++i;
            pos += 3;
        } else if (NAL_HEADER_PREFIX((data+pos))) {
            index[i] = pos;
            ++i;
            pos += 4;
        }
        ++pos;
    }
    if (i == 0) {
        return -1;
    }
    for (j = 0; j < i; ++j) {
        pos = index[j];
        uint8_t *nal = &data[pos];
        int nal_type = nal[4] &  0x1F;
        switch (nal_type) {
        case H264_NAL_SLICE:
            break;
        case H264_NAL_IDR_SLICE:
            break;
        case H264_NAL_SEI:
            break;
        case H264_NAL_SPS:
            got_sps = 1;
            break;
        case H264_NAL_PPS:
            got_pps = 1;
            break;
        default:
            loge("nal_type=%d\n", nal_type);
            break;
        }
        if (got_sps && got_pps) {
            return index[j+1];
        }
    }
    return 0;
}

static int find_nalu_header_pos(uint8_t *data, int len, int start)
{
    int pos = start;
    while (pos < len - NAL_HEADER_LEN) {
        if (NAL_HEADER_PREFIX((data+pos))) {
            break;
        }
        ++pos;
    }
    if (pos == len - NAL_HEADER_LEN) {
        pos = -1;
    }
    return pos;
}

static int get_extra_data(uint8_t *data, int data_len, uint8_t *extra_data, int *extra_len)
{
    int pos = 0;
    int nal_pos = 0;

    *extra_len = 0;
    while (pos < data_len) {
        pos = find_nalu_header_pos(data, data_len, nal_pos + 4);
        if (pos == -1 || pos >= data_len -4) {
            pos = data_len;
        }
        int nal_len = pos - nal_pos;
        uint8_t *nal = data + nal_pos;
        switch (NAL_TYPE(nal)) {
        case H264_NAL_SPS:
            memcpy(extra_data, nal, nal_len);
            *extra_len = nal_len;
            break;
        case H264_NAL_PPS:
            memcpy(&extra_data[*extra_len], nal, nal_len);
            *extra_len += nal_len;
            return 0;
            break;
        default:
            loge("got nal_type=%d, nal=0x%X\n", NAL_TYPE(nal), nal[4]);
            break;
        }
        nal_pos = pos;
    }
    if (*extra_len == 0) {
        return -1;
    }
    return 0;
}

static int rtmp_write(struct protocol_ctx *pc, void *buf, int len)
{
    struct rtmp_ctx *rc = (struct rtmp_ctx *)pc->priv;
    int ret = 0;

    if (rc->status == RTMP_STATUS_IDLE) {
#ifdef USE_RTMPCLIENT
        if (!rc->client->is_running()) {
            loge("is not running\n");
            return 0;
        }
#endif
        unsigned char extra_data[128];
        int extra_data_len = 0;
        if (!get_extra_data((uint8_t *)buf, len, extra_data, &extra_data_len)) {
#ifdef USE_RTMPCLIENT
            rc->client->add_h264(extra_data, extra_data_len, 0);
            rc->client->set_url((char *)rc->url);
#else
            struct iovec data = {
                .iov_base = buf,
                .iov_len = len,
            };
            rtmp_stream_add(rc->client, RTMP_DATA_H264, &data);
            rtmp_stream_start(rc->client);
#endif
            rc->status = RTMP_STATUS_RUNNING;
        } else {
            loge("get_extra_data failed!\n");
        }
    } else if (rc->status == RTMP_STATUS_RUNNING) {
        int offset = strip_nalu_sps_pps((uint8_t *)buf, len);
        buf = (void *)((uint8_t *)buf + offset);
        len -= offset;
#ifdef USE_RTMPCLIENT
        rc->client->send_data(RtmpClient::TYPE_H264, (unsigned char *)buf, len, 0);
#else

        rtmp_send_data(rc->client, RTMP_DATA_H264, (uint8_t *)buf, len, 0);
#endif
    }
    return ret;
}

static void rtmp_close(struct protocol_ctx *pc)
{
    struct rtmp_ctx *rc = (struct rtmp_ctx *)pc->priv;
#ifdef USE_RTMPCLIENT
    delete rc->client;
#else
    rtmp_stream_stop(rc->client);

#endif
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
