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
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <x264.h>
#ifdef __cplusplus
}
#endif


#include <libmacro.h>
#include <liblog.h>

#include "codec.h"
#include "common.h"
#include "imgconvert.h"

struct x264_ctx {
    enum video_format input_format;
    struct iovec sei;
    x264_param_t param;
    x264_t *handle;
    bool first_frame;
    bool append_extra;
    uint64_t cur_pts;
    uint32_t timebase_num;
    uint32_t timebase_den;
    struct iovec extradata;
    void *parent;
};

#define AV_INPUT_BUFFER_PADDING_SIZE 32

static int video_format_to_x264_csp(enum video_format fmt)
{
    switch (fmt) {
    case VIDEO_FORMAT_UYVY:
        return X264_CSP_UYVY;
    case VIDEO_FORMAT_YUY2:
        return X264_CSP_YUYV;
    case VIDEO_FORMAT_NV12:
        return X264_CSP_NV12;
    case VIDEO_FORMAT_I420:
        return X264_CSP_I420;
    case VIDEO_FORMAT_I444:
        return X264_CSP_I444;
    default:
        return X264_CSP_NONE;
    }
}

static int init_header(struct x264_ctx *c)
{
    x264_nal_t *nal;
    uint8_t *p;
    int nal_cnt, nal_bytes, i;

    nal_bytes = x264_encoder_headers(c->handle, &nal, &nal_cnt);
    if (nal_bytes < 0) {
        loge("x264_encoder_headers failed!\n");
        return -1;
    }

    c->extradata.iov_base = p = (uint8_t *)calloc(1, nal_bytes + AV_INPUT_BUFFER_PADDING_SIZE);
    if (!p)
        return -1;

    for (i = 0; i < nal_cnt; i++) {
        /* Don't put the SEI in extradata. */
        if (nal[i].i_type == NAL_SEI) {
            logd("%s\n", nal[i].p_payload+25);
            c->sei.iov_len = nal[i].i_payload;
            c->sei.iov_base = memdup(nal[i].p_payload, nal[i].i_payload);
            if (!c->sei.iov_base)
                return -1;
        } else {
            memcpy(p, nal[i].p_payload, nal[i].i_payload);
            p += nal[i].i_payload;
        }
    }
    c->extradata.iov_len = p - (uint8_t *)c->extradata.iov_base;
    return 0;
}

static int _x264_open(struct codec_ctx *cc, struct media_params *media)
{
    struct x264_ctx *c = CALLOC(1, struct x264_ctx);
    if (!c) {
        loge("malloc x264_ctx failed!\n");
        return -1;
    }

    x264_param_default_preset(&c->param, "ultrafast" , "zerolatency");
    c->input_format = media->video.format;

    c->param.rc.i_vbv_max_bitrate = 2500;
    c->param.rc.i_vbv_buffer_size = 2500;
    c->param.rc.i_bitrate = 2500;
    c->param.rc.i_rc_method = X264_RC_ABR;
    c->param.rc.b_filler = true;
    c->param.i_keyint_max = 30;
    //XXX b_repeat_headers 0: rtmp is ok; 1: playback is ok
    c->param.b_repeat_headers = 0;  // repeat SPS/PPS before i frame
    c->param.b_vfr_input = 0;
    c->param.i_log_level = X264_LOG_INFO;
    c->param.i_csp = video_format_to_x264_csp(c->input_format);

    c->param.i_width = media->video.width;
    c->param.i_height = media->video.height;
    c->param.i_fps_num = media->video.fps_num;
    c->param.i_fps_den = media->video.fps_den;

    x264_param_apply_profile(&c->param, NULL);

    c->handle = x264_encoder_open(&c->param);
    if (c->handle == 0) {
        loge("x264_encoder_open failed!\n");
        goto failed;
    }

    c->first_frame = true;
    c->append_extra = false;
    c->timebase_num = c->param.i_fps_den;
    c->timebase_den = c->param.i_fps_num;

    init_header(c);

    cc->extradata.iov_base = c->extradata.iov_base;
    cc->extradata.iov_len = c->extradata.iov_len;
    c->parent = cc;
    cc->priv = c;
    return 0;

failed:
    if (c->handle) {
        x264_encoder_close(c->handle);
        c->handle = 0;
    }
    if (c) {
        free(c);
    }
    return -1;
}

static int init_pic_data(struct x264_ctx *c, x264_picture_t *pic,
                struct video_frame *frame)
{
    x264_picture_init(pic);
    pic->i_pts = frame->timestamp;
    pic->img.i_csp = c->param.i_csp;

    switch (c->param.i_csp) {
    case X264_CSP_YUYV:
        pic->img.i_plane = 1;
        break;
    case X264_CSP_NV12:
        pic->img.i_plane = 2;
        break;
    case X264_CSP_I420:
    case X264_CSP_I444:
        pic->img.i_plane = 3;
        break;
    default:
        loge("unsupport colorspace type\n");
        break;
    }
    if (pic->img.i_plane != frame->planes) {
        loge("video frame planes mismatch\n");
        return -1;
    }

    for (int i = 0; i < pic->img.i_plane; i++) {
        pic->img.i_stride[i] = (int)frame->linesize[i];
        pic->img.plane[i] = frame->data[i];
    }
    return 0;
}

static int fill_packet(struct x264_ctx *c, struct video_packet *pkt,
                       x264_nal_t *nals, int nal_cnt, x264_picture_t *pic_out)
{
    uint8_t *p;
    int i;
    size_t size = c->sei.iov_len;

    for (i = 0; i < nal_cnt; i++)
        size += nals[i].i_payload;

    if (pkt->size < size) {
        pkt->data = realloc(pkt->data, size);
    }
    if (!pkt->data) {
        return -1;
    }

    pkt->size = 0;
    p = pkt->data;

    if (!c->append_extra) {
        /* Write the extradata as part of the first frame. */
        if (c->extradata.iov_len > 0 && nal_cnt > 0) {
            if (c->extradata.iov_len > size) {
                loge("Error: nal buffer is too small\n");
                return -1;
            }
            memcpy(p, c->extradata.iov_base, c->extradata.iov_len);
            p += c->extradata.iov_len;
            pkt->size += c->extradata.iov_len;
        }
        c->append_extra = true;
    }

    for (i = 0; i < nal_cnt; i++){
        memcpy(p, nals[i].p_payload, nals[i].i_payload);
        p += nals[i].i_payload;
        pkt->size += nals[i].i_payload;
    }

    pkt->pts = pic_out->i_pts;
    pkt->dts = pic_out->i_dts;
    pkt->timebase_num = c->param.i_fps_den;
    pkt->timebase_den = c->param.i_fps_num;
    pkt->key_frame = pic_out->b_keyframe != 0;
    logd("pkt->pts=%zu, dts=%zu, keyframe=%d\n", pkt->pts, pkt->dts, pkt->key_frame);

    return pkt->size;
}

static int _x264_encode(struct codec_ctx *cc, struct iovec *in, struct iovec *out)
{
    struct x264_ctx *c = (struct x264_ctx *)cc->priv;
    x264_picture_t pic_in, pic_out;
    x264_nal_t *nal;
    int nal_cnt = 0;
    int ret = 0;
    int nal_bytes = 0;
    struct video_frame *frm = in->iov_base;
    struct video_packet *pkt = out->iov_base;
    if (c->first_frame) {
        c->cur_pts = 0;
        c->first_frame = false;
    }
    frm->timestamp = c->cur_pts;

    init_pic_data(c, &pic_in, frm);

    nal_bytes = x264_encoder_encode(c->handle, &nal, &nal_cnt, &pic_in,
                    &pic_out);
    if (nal_bytes < 0) {
        loge("x264_encoder_encode failed!\n");
        return -1;
    }
    ret = fill_packet(c, pkt, nal, nal_cnt, &pic_out);
    if (ret < 0) {
        loge("fill_packet failed!\n");
        return -1;
    }
    c->cur_pts += c->timebase_num;
    out->iov_len = ret;
    return ret;
}

static void _x264_close(struct codec_ctx *cc)
{
    struct x264_ctx *c = (struct x264_ctx *)cc->priv;
    if (c->handle) {
        x264_encoder_close(c->handle);
        free(cc->extradata.iov_base);
        cc->extradata.iov_len = 0;
    }
    free(c->sei.iov_base);
    free(c);
}

struct codec aq_x264_encoder = {
    .name   = "x264",
    .open   = _x264_open,
    .encode = _x264_encode,
    .decode = NULL,
    .close  = _x264_close,
};
