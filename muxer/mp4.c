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
#include <x264.h>
#include <libmacro.h>
#include <liblog.h>

#include "muxer.h"
#include "common.h"


#ifdef __cplusplus
extern "C" {
#endif

#define __STDC_CONSTANT_MACROS
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>

#ifdef __cplusplus
}
#endif


enum gear_video_type {
    GEAR_VIDEO_FRAME_I = 0,
    GEAR_VIDEO_FRAME_IDR,
    GEAR_VIDEO_FRAME_P,
    GEAR_VIDEO_FRAME_B,
};

enum gear_frame_type {
    GEAR_FRAME_AUDIO = 0,
    GEAR_FRAME_VIDEO,
};


struct gear_video_param {
    enum gear_video_type type;
};

struct gear_audio_param {
    int sample_rate;
    int channel;
    int bitrate;
};

struct gear_frame {
    struct iovec data;
    gear_frame_type type;
    uint64_t pts;
    union {
        struct gear_video_param video;
        struct gear_audio_param audio;
    } param;
};

struct muxer_media {
    enum AVCodecID codec_id;
    AVStream *av_stream;
    AVCodec *av_codec;
    AVBitStreamFilterContext *av_bsf;
    uint64_t first_pts;
    uint64_t last_pts;
};

struct mp4_muxer_ctx {
    AVFormatContext *av_format;
    struct muxer_media audio;
    struct muxer_media video;
    bool got_video;
};

#define BITRATE     1000000
#define WIDTH       640
#define HEIGHT      480
#define FRAME_RATE  30

static int muxer_add_stream(struct mp4_muxer_ctx *muxer, struct muxer_media *media, enum AVCodecID codec_id)
{
    AVCodec *codec = avcodec_find_encoder(codec_id);
    if (!codec) {
        loge("Could not find encoder for '%s'\n", avcodec_get_name(codec_id));
        return -1;
    }
    media->first_pts = 0;
    media->last_pts = 0;

    media->av_stream = avformat_new_stream(muxer->av_format, codec);
    if (!media->av_stream) {
        loge("Could not allocate stream\n");
        return -1;
    }
    media->av_stream->id = muxer->av_format->nb_streams-1;
    AVCodecContext *c = media->av_stream->codec;

    switch (codec->type) {
    case AVMEDIA_TYPE_AUDIO:
        c->sample_fmt  = AV_SAMPLE_FMT_FLTP;
        c->bit_rate    = 32000;
        c->sample_rate = 8000;
        c->channel_layout = AV_CH_LAYOUT_STEREO;
        c->channels       = av_get_channel_layout_nb_channels(c->channel_layout);
        c->frame_size  = 1000;
        media->av_stream->time_base = (AVRational){ 1, c->sample_rate };
        muxer->audio.av_bsf = av_bitstream_filter_init("aac_adtstoasc");
        break;
    case AVMEDIA_TYPE_VIDEO:
        codec_id = AV_CODEC_ID_H264;
        if (codec_id != AV_CODEC_ID_H264 && codec_id != AV_CODEC_ID_H265 &&
            codec_id != AV_CODEC_ID_HEVC && codec_id != AV_CODEC_ID_MJPEG) {
            loge("video codec_id %s not support\n", avcodec_get_name(codec_id));
            return -1;
        }
        c->codec_id       = codec_id;
        c->bit_rate       = BITRATE;
        c->width          = WIDTH;
        c->height         = HEIGHT;
        media->av_stream->time_base = (AVRational){ 1, FRAME_RATE };
        c->time_base      = (AVRational){ 1, FRAME_RATE };
        c->gop_size       = 12;
        c->pix_fmt        = AV_PIX_FMT_NV12;
        break;
    default:
        loge("codec type not support\n");
        break;
    }

    if (muxer->av_format->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    return 0;
}

static int ffmpeg_muxer_init(struct mp4_muxer_ctx *c)
{
    AVOutputFormat *ofmt = NULL;

    av_register_all();
    av_log_set_level(AV_LOG_DEBUG);
    avformat_alloc_output_context2(&c->av_format, NULL, "mp4", NULL);
    if (c->av_format == NULL) {
        loge("avformat_alloc_output_context2 failed\n");
        return -1;
    }

    ofmt = c->av_format->oformat;
    if (ofmt->video_codec != AV_CODEC_ID_NONE) {
        muxer_add_stream(c, &c->video, ofmt->video_codec);
    }
    if (ofmt->audio_codec != AV_CODEC_ID_NONE) {
        muxer_add_stream(c, &c->audio, ofmt->audio_codec);
    }
    c->got_video = false;
    return 0;
}

static int mp4_open(struct muxer_ctx *mc, struct media_params *media)
{
    struct mp4_muxer_ctx *c = CALLOC(1, struct mp4_muxer_ctx);
    if (!c) {
        loge("malloc mp4_muxer_ctx failed!\n");
        return -1;
    }
    if (0 != ffmpeg_muxer_init(c)) {
        loge("ffmpeg_muxer_init failed!\n");
        goto failed;
    }

    if (!(c->av_format->oformat->flags & AVFMT_NOFILE)) {
        if (0 > avio_open(&c->av_format->pb, mc->url.body, AVIO_FLAG_WRITE)) {
            loge("Could not open '%s'\n", mc->url.body);
            goto failed;
        }
    }

    if (0 > avformat_write_header(c->av_format, NULL)) {
        loge("write header error\n");
        goto failed;
    }
    mc->priv = c;
    return 0;

failed:
    if (c) {
        free(c);
    }
    return -1;
}

#if 1
static int64_t update_video_timestamp(struct mp4_muxer_ctx *c, uint64_t pts)
{
    if (c->video.first_pts == 0) {
        c->video.first_pts = pts;
    }
    if (pts < c->video.last_pts) {
        c->video.last_pts = c->video.last_pts + 1;
    } else {
        c->video.last_pts = pts - c->video.first_pts;
    }
    return av_rescale_q(c->video.last_pts, (AVRational){1, 1000}, (AVRational){1, 12500});//fps=25/2
}
#endif

static int mp4_write(struct muxer_ctx *cc, struct packet *pkt)
{
    struct mp4_muxer_ctx *c = (struct mp4_muxer_ctx *)cc->priv;
    AVPacket avpkt;
    av_init_packet(&avpkt);
    bool invalid = false;
    int ret = 0;

    switch (pkt->type) {
    case MEDIA_TYPE_AUDIO:
        if (c->got_video == false) {
            goto failed;
        }
        avpkt.stream_index = c->audio.av_stream->index;
        avpkt.data = (uint8_t *)pkt->data.iov_base;
        avpkt.size = pkt->data.iov_len;
        avpkt.pos = -1;
        //avpkt.pts = update_audio_timestamp(c, pkt->pts);

        av_bitstream_filter_filter(c->audio.av_bsf, c->audio.av_stream->codec, NULL, &avpkt.data, &avpkt.size, avpkt.data, avpkt.size, 0);

        break;
    case MEDIA_TYPE_VIDEO:
#if 0
        if (!valid_video_frame(avpkt)) {
            loge("video pkt is invalid type!\n");
            goto failed;
        }
        if (pkt->param.video.type == GEAR_VIDEO_FRAME_I) {
            c->got_video = true;
            pkt.flags |= AV_PKT_FLAG_KEY;
        }
        if (c->got_video == false) {
            goto failed;
        }
#endif
        avpkt.stream_index = c->video.av_stream->index;
        avpkt.data = (uint8_t *)pkt->data.iov_base;
        avpkt.size = pkt->data.iov_len;
        avpkt.pos = -1;
        loge("size = %d, pts = %d\n", avpkt.size, avpkt.pts);
        avpkt.pts = update_video_timestamp(c, pkt->pts);
        //avpkt.pts++;
        break;
    default:
        loge("unknown pkt type\n");
        invalid = true;
        break;
    }

    if (invalid) {
        goto failed;
    }
//    if (c->got_video) {
        loge("xxx\n");
        ret = av_interleaved_write_frame(c->av_format, &avpkt);
        loge("ret = %d\n", ret);
//    }
failed:
    av_packet_unref(&avpkt);
    return 0;
}

void mp4_close(struct muxer_ctx *cc)
{
    loge("xxx\n");
    struct mp4_muxer_ctx *c = (struct mp4_muxer_ctx *)cc->priv;
    if (0 > av_write_trailer(c->av_format)) {
        loge("av_write_trailer failed!\n");
    }
    loge("xxx\n");
    av_bitstream_filter_close(c->audio.av_bsf);
    loge("xxx\n");
    if (!(c->av_format->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&c->av_format->pb);
    }
    loge("xxx\n");
    avcodec_close(c->audio.av_stream->codec);
    loge("xxx\n");
    avcodec_close(c->video.av_stream->codec);
    loge("xxx\n");
    avformat_free_context(c->av_format);
    loge("xxx\n");
    free(c);
    loge("xxx\n");
}

struct muxer aq_mp4_muxer = {
    .name         = "mp4",
    .open         = mp4_open,
    .write_header = NULL,
    .write_packet = mp4_write,
    .write_tailer = NULL,
    .read_header  = NULL,
    .read_packet  = NULL,
    .read_tailer  = NULL,
    .close        = mp4_close,
};
