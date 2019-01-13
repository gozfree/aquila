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

struct h264dec_ctx {
    int width;
    int height;
    AVCodecContext *avctx;
    AVCodec *avcdc;
    AVFrame *ori_avfrm;
    AVFrame *cvt_avfrm;
};

static int h264dec_open(struct codec_ctx *cc, struct media_params *media)
{
    struct h264dec_ctx *c = CALLOC(1, struct h264dec_ctx);
    if (!c) {
        loge("malloc h264dec_ctx failed!\n");
        return -1;
    }
    int pic_size;
    uint8_t *out_buffer;
    c->ori_avfrm = (AVFrame *)av_mallocz(sizeof(AVFrame));
    c->cvt_avfrm = (AVFrame *)av_mallocz(sizeof(AVFrame));
    av_register_all();
    AVCodec *avcdc = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!avcdc) {
        printf("avcodec_find_decoder failed!\n");
        return -1;
    }
    AVCodecContext *avctx = avcodec_alloc_context3(avcdc);
    if (!avctx) {
        printf("can not alloc avcodec context!\n");
        return -1;
    }
#if 0
    /*configure codec context */
    avctx->time_base.num = 1;
    avctx->time_base.den = 25;
    avctx->bit_rate = 0;
    avctx->frame_number = 1;//one frame per package
    avctx->codec_type = AVMEDIA_TYPE_VIDEO;
#endif
    avctx->width = media->video.width;
    avctx->height = media->video.height;

    /*open codec*/
    if (0 > avcodec_open2(avctx, avcdc, NULL)) {
        printf("can not open avcodec!\n");
        return -1;
    }

/*preparation for frame convertion*/
    pic_size = avpicture_get_size(AV_PIX_FMT_YUV420P, avctx->width, avctx->height);
    out_buffer = (uint8_t *)calloc(1, pic_size);
    avpicture_fill((AVPicture *)c->cvt_avfrm, out_buffer, AV_PIX_FMT_YUV420P, avctx->width, avctx->height);
    c->width = media->video.width;
    c->height = media->video.height;
    c->avctx = avctx;
    c->avcdc = avcdc;
    cc->priv = c;
    return 0;
}

static void frame_conv(struct h264dec_ctx *c)
{
    struct SwsContext *swsctx;
    swsctx = sws_getContext(c->avctx->width, c->avctx->height, c->avctx->pix_fmt, c->avctx->width, c->avctx->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(swsctx, (const uint8_t* const*)c->ori_avfrm->data, c->ori_avfrm->linesize, 0, c->avctx->height, c->cvt_avfrm->data, c->cvt_avfrm->linesize);
    sws_freeContext(swsctx);
}

static int h264dec_decode(struct codec_ctx *cc, struct iovec *in, struct iovec *out)
{
    struct h264dec_ctx *c = (struct h264dec_ctx *)cc->priv;
    int got_pic = 0;
    AVPacket avpkt;
    memset(&avpkt, 0, sizeof(AVPacket));
    avpkt.size = in->iov_len;
    avpkt.data = (uint8_t *)in->iov_base;

    avcodec_decode_video2(c->avctx, c->ori_avfrm, &got_pic, &avpkt);
    if (got_pic) {
        frame_conv(c);
    }
    out->iov_len = c->cvt_avfrm->linesize[0] + c->cvt_avfrm->linesize[1] + c->cvt_avfrm->linesize[2];
#if 0
    out->iov_base = CALLOC(out->iov_len, void);
    memcpy(out->iov_base, c->cvt_avfrm->data[0], c->cvt_avfrm->linesize[0]);
    memcpy((void *)((uint8_t *)out->iov_base + c->cvt_avfrm->linesize[0]),
                          c->cvt_avfrm->data[1], c->cvt_avfrm->linesize[1]);
    memcpy((void *)((uint8_t *)out->iov_base + c->cvt_avfrm->linesize[0] + c->cvt_avfrm->linesize[1]),
                          c->cvt_avfrm->data[2], c->cvt_avfrm->linesize[2]);
    //DUMP_BUFFER(out->iov_base, c->cvt_avfrm->linesize[0]);
    //DUMP_BUFFER((uint8_t *)out->iov_base + c->cvt_avfrm->linesize[0], c->cvt_avfrm->linesize[1]);
    //DUMP_BUFFER((uint8_t *)out->iov_base + c->cvt_avfrm->linesize[0] + c->cvt_avfrm->linesize[1], c->cvt_avfrm->linesize[2]);
#else
    out->iov_base = c->cvt_avfrm;
    //DUMP_BUFFER(c->cvt_avfrm->data[0], c->cvt_avfrm->linesize[0]);
    //DUMP_BUFFER(c->cvt_avfrm->data[1], c->cvt_avfrm->linesize[1]);
    //DUMP_BUFFER(c->cvt_avfrm->data[2], c->cvt_avfrm->linesize[2]);
#endif
    logd("after decode %d, %d, %d\n", c->cvt_avfrm->linesize[0], c->cvt_avfrm->linesize[1], c->cvt_avfrm->linesize[2]);

    return out->iov_len;
}

static void h264dec_close(struct codec_ctx *cc)
{
    struct h264dec_ctx *c = (struct h264dec_ctx *)cc->priv;
    //TODO:
    free(c);
}

struct codec aq_h264dec_decoder = {
    .name   = "h264dec",
    .open   = h264dec_open,
    .encode = NULL,
    .decode = h264dec_decode,
    .close  = h264dec_close,
};
