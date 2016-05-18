/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    x264_enc.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-04 00:55
 * updated: 2016-05-04 00:55
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <x264.h>
#include <libgzf.h>
#include <liblog.h>

#include "codec.h"
#include "common.h"

struct x264_ctx {
    int width;
    int height;
    x264_param_t *param;
    x264_t *handle;
    x264_picture_t *picture;
    x264_nal_t *nal;
};

static int x264_open(struct codec_ctx *cc, struct media_params *media)
{
    int m_frameRate = 25;
    //int m_bitRate = 1000;
    struct x264_ctx *c = CALLOC(1, struct x264_ctx);
    if (!c) {
        loge("malloc x264_ctx failed!\n");
        return -1;
    }
    c->param = CALLOC(1, x264_param_t);
    if (!c->param) {
        loge("malloc param failed!\n");
        goto failed;
    }
    c->picture = CALLOC(1, x264_picture_t);
    if (!c->picture) {
        loge("malloc picture failed!\n");
        goto failed;
    }
    x264_param_default_preset(c->param, "ultrafast" , "zerolatency");

    c->param->i_width = media->video.width;
    c->param->i_height = media->video.height;
    c->param->b_repeat_headers = 1;  // repeat SPS/PPS before i frame
    c->param->b_cabac = 1;
    c->param->i_threads = 1;
    c->param->i_fps_num = (int)m_frameRate;
    c->param->i_fps_den = 1;
    c->param->i_keyint_max = 128;
    c->param->i_log_level = X264_LOG_NONE;
    c->handle = x264_encoder_open(c->param);
    if (c->handle == 0) {
        loge("x264_encoder_open failed!\n");
        goto failed;
    }

    if (-1 == x264_picture_alloc(c->picture, X264_CSP_I420, c->param->i_width,
            c->param->i_height)) {
        loge("x264_picture_alloc failed!\n");
        goto failed;
    }
    c->picture->img.i_csp = X264_CSP_I420;
    c->picture->img.i_plane = 3;
    cc->priv = c;
    return 0;

failed:
    if (c->handle) {
        x264_encoder_close(c->handle);
    }
    if (c) {
        free(c);
    }
    return -1;
}

static int x264_encode(struct codec_ctx *cc, struct iovec *in, struct iovec *out)
{
    struct x264_ctx *c = (struct x264_ctx *)cc->priv;
    x264_picture_t pic_out;
    int nNal = 0;
    int bit_len = 0;
    int i = 0, j = 0;
    uint8_t *p_out;
    c->nal = NULL;
    uint8_t *p422;

    uint8_t *y = c->picture->img.plane[0];
    uint8_t *u = c->picture->img.plane[1];
    uint8_t *v = c->picture->img.plane[2];

    int widthStep422 = c->param->i_width * 2;

    for(i = 0; i < c->param->i_height; i += 2) {
        p422 = (uint8_t *)in->iov_base + i * widthStep422;
        for(j = 0; j < widthStep422; j+=4) {
            *(y++) = p422[j];
            *(u++) = p422[j+1];
            *(y++) = p422[j+2];
        }

        p422 += widthStep422;

        for(j = 0; j < widthStep422; j+=4) {
            *(y++) = p422[j];
            *(v++) = p422[j+3];
            *(y++) = p422[j+2];
        }
    }
    c->picture->i_type = X264_TYPE_I;

    if (x264_encoder_encode(c->handle, &(c->nal), &nNal, c->picture,
                &pic_out) < 0) {
        return -1;
    }
    c->picture->i_pts++;

    for (i = 0; i < nNal; i++) {
        bit_len += c->nal[i].i_payload;
    }
    p_out = (uint8_t *)calloc(1, bit_len);
    if (!p_out) {
        return -1;
    }
    out->iov_base = p_out;
    for (i = 0; i < nNal; i++) {
        memcpy(p_out, c->nal[i].p_payload, c->nal[i].i_payload);
        p_out += c->nal[i].i_payload;
    }
    out->iov_len = bit_len;
    return bit_len;
}

static void x264_close(struct codec_ctx *cc)
{
    struct x264_ctx *c = (struct x264_ctx *)cc->priv;
    if (c->handle) {
        x264_encoder_close(c->handle);
    }
    free(c);
}

struct codec aq_x264_encoder = {
    .name   = "x264",
    .open   = x264_open,
    .encode = x264_encode,
    .decode = NULL,
    .close  = x264_close,
};
