/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    h264_dec.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-20 00:38
 * updated: 2016-05-20 00:38
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>

#include <libgzf.h>
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
    AVCodec *avcdc = avcodec_find_decoder(CODEC_ID_H264);
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
    pic_size = avpicture_get_size(PIX_FMT_YUV420P, avctx->width, avctx->height);
    out_buffer = (uint8_t *)calloc(1, pic_size);
    avpicture_fill((AVPicture *)c->cvt_avfrm, out_buffer, PIX_FMT_YUV420P, avctx->width, avctx->height);
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
    swsctx = sws_getContext(c->avctx->width, c->avctx->height, c->avctx->pix_fmt, c->avctx->width, c->avctx->height, PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);
    sws_scale(swsctx, (const uint8_t* const*)c->ori_avfrm->data, c->ori_avfrm->linesize, 0, c->avctx->height, c->cvt_avfrm->data, c->cvt_avfrm->linesize);
    sws_freeContext(swsctx);
}

static int h264dec_decode(struct codec_ctx *cc, struct iovec *in, struct iovec *out)
{
    struct h264dec_ctx *c = cc->priv;
    int got_pic = 0;
    AVPacket avpkt;
    memset(&avpkt, 0, sizeof(AVPacket));
    avpkt.size = in->iov_len;
    avpkt.data = (uint8_t *)in->iov_base;

    avcodec_decode_video2(c->avctx, c->ori_avfrm, &got_pic, &avpkt);
    if (got_pic) {
        frame_conv(c);
    }
    out->iov_base = (void *)c->cvt_avfrm;
    out->iov_len = got_pic;
    return got_pic;
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
