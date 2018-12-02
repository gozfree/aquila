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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#ifdef __cplusplus
}
#endif

#include <libmacro.h>
#include <liblog.h>
#include "codec.h"
#include "common.h"

struct h264enc_ctx {
    int width;
    int height;
    AVCodecContext *avctx;
    AVCodec *avcdc;
    AVFrame *avfrm;
};

static AVFrame *alloc_picture(enum AVPixelFormat pix_fmt, int width, int height)
{
    int ret;

    AVFrame *frame = av_frame_alloc();
    if (!frame)
        return NULL;

    frame->format = pix_fmt;
    frame->width  = width;
    frame->height = height;

    ret = av_image_alloc(frame->data, frame->linesize, width, height, pix_fmt, 32);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate raw picture buffer\n");
        exit(1);
    }
    return frame;
}

static int h264enc_open(struct codec_ctx *cc, struct media_params *media)
{
    struct h264enc_ctx *c = CALLOC(1, struct h264enc_ctx);
    if (!c) {
        loge("malloc h264enc_ctx failed!\n");
        return -1;
    }
    avcodec_register_all();
    AVCodec *avcdc = avcodec_find_encoder(CODEC_ID_H264);
    if (!avcdc) {
        printf("avcodec_find_encoder failed!\n");
        return -1;
    }
    AVCodecContext *avctx = avcodec_alloc_context3(avcdc);
    if (!avctx) {
        printf("can not alloc avcodec context!\n");
        return -1;
    }

    avctx->pix_fmt = PIX_FMT_YUV420P;
    avctx->width = media->video.width;
    avctx->height = media->video.height;

    avctx->bit_rate = 4000000;
    avctx->time_base.num=1;
    avctx->time_base.den=25;
    avctx->gop_size = 10;
    avctx->max_b_frames = 1;

    /*open codec*/
    if (0 > avcodec_open2(avctx, avcdc, NULL)) {
        printf("can not open avcodec!\n");
        return -1;
    }
    c->avfrm = alloc_picture(avctx->pix_fmt, avctx->width, avctx->height);

    c->width = media->video.width;
    c->height = media->video.height;
    c->avctx = avctx;
    c->avcdc = avcdc;
    cc->priv = c;
    return 0;
}


static int h264enc_encode(struct codec_ctx *cc, struct iovec *in, struct iovec *out)
{
    struct h264enc_ctx *c = (struct h264enc_ctx *)cc->priv;
    int got_pic = 0;
    int ret = 0;
    AVPacket avpkt;
    av_init_packet(&avpkt);
    avpkt.size = out->iov_len;
    avpkt.data = (uint8_t *)out->iov_base;

#if 1
    uint8_t *p422;
    int i = 0, j = 0;

    uint8_t *y = c->avfrm->data[0];
    uint8_t *u = c->avfrm->data[1];
    uint8_t *v = c->avfrm->data[2];

    c->avfrm->linesize[0] = c->width;
    c->avfrm->linesize[1] = c->width / 2;
    c->avfrm->linesize[2] = c->width / 2;

    int widthStep422 = c->width * 2;

    for(i = 0; i < c->height; i += 2) {
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
#endif
    c->avfrm->pts++;

    loge("enter in len = %d, pts=%d\n", in->iov_len, c->avfrm->pts);
    ret = avcodec_encode_video2(c->avctx, &avpkt, c->avfrm, &got_pic);
    //                   ctx      , output, in          , out
    loge("leave out got_pic = %d, len = %d, ret=%d\n", got_pic, out->iov_len, ret);
    return ret;
}

static void h264enc_close(struct codec_ctx *cc)
{
    struct h264enc_ctx *c = (struct h264enc_ctx *)cc->priv;
    //TODO:
    free(c);
}
struct codec aq_h264enc_encoder = {
    .name   = "h264enc",
    .open   = h264enc_open,
    .encode = h264enc_encode,
    .decode = NULL,
    .close  = h264enc_close,
};
