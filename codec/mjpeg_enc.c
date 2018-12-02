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
#include <stdarg.h>
#include <stdint.h>
#include <sys/uio.h>
#include <jpeglib.h>
#include <libmacro.h>
#include <liblog.h>

#include "codec.h"
#include "common.h"
#include "imgconvert.h"

#define COLOR_COMPONENTS    (3)
#define OUTPUT_BUF_SIZE     (4096)
#define DEFAULT_JPEG_QUALITY	(60)

struct mjpeg_ctx {
    int width;
    int height;
    int encode_format;
    int input_format;
    struct jpeg_compress_struct encoder;
    struct jpeg_error_mgr errmgr;
};

typedef struct jpeg_args {
    struct jpeg_destination_mgr pub; /* public fields */
    JOCTET * buffer;    /* start of buffer */
    unsigned char *outbuffer;
    int outbuffer_size;
    unsigned char *outbuffer_cursor;
    int *written;
} jpeg_args_t;

static int mjpeg_open(struct codec_ctx *c, struct media_params *media)
{
    struct mjpeg_ctx *mc = CALLOC(1, struct mjpeg_ctx);
    if (!mc) {
        loge("malloc mjpeg_ctx failed!\n");
        return -1;
    }
    mc->width = media->video.width;
    mc->height = media->video.height;
    mc->input_format = media->video.pix_fmt;
    mc->encode_format = YUV420;
    mc->encoder.err = jpeg_std_error(&mc->errmgr);
    jpeg_create_compress(&mc->encoder);
    jpeg_set_quality(&mc->encoder, DEFAULT_JPEG_QUALITY, 1);
    c->priv = mc;

    return 0;
}

static void init_destination(j_compress_ptr cinfo)
{
    struct jpeg_args *dest = (struct jpeg_args*) cinfo->dest;
    dest->buffer = (JOCTET *)(*cinfo->mem->alloc_small)((j_common_ptr) cinfo,
                    JPOOL_IMAGE, OUTPUT_BUF_SIZE * sizeof(JOCTET));
    *(dest->written) = 0;
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
}

static boolean empty_output_buffer(j_compress_ptr cinfo)
{
    struct jpeg_args *dest = (struct jpeg_args *) cinfo->dest;
    memcpy(dest->outbuffer_cursor, dest->buffer, OUTPUT_BUF_SIZE);
    dest->outbuffer_cursor += OUTPUT_BUF_SIZE;
    *(dest->written) += OUTPUT_BUF_SIZE;
    dest->pub.next_output_byte = dest->buffer;
    dest->pub.free_in_buffer = OUTPUT_BUF_SIZE;
    return TRUE;
}

static void term_destination(j_compress_ptr cinfo)
{
    struct jpeg_args * dest = (struct jpeg_args *) cinfo->dest;
    size_t datacount = OUTPUT_BUF_SIZE - dest->pub.free_in_buffer;
    memcpy(dest->outbuffer_cursor, dest->buffer, datacount);
    dest->outbuffer_cursor += datacount;
    *(dest->written) += datacount;
}
static void dest_buffer(j_compress_ptr cinfo, void *buffer, int size, int *written)
{
    struct jpeg_args * dest;
    if (cinfo->dest == NULL) {
        cinfo->dest = (struct jpeg_destination_mgr *)(*cinfo->mem->alloc_small)
             ((j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(struct jpeg_args));
    }

    dest = (struct jpeg_args*) cinfo->dest;
    dest->pub.init_destination = init_destination;
    dest->pub.empty_output_buffer = empty_output_buffer;
    dest->pub.term_destination = term_destination;
    dest->outbuffer = (unsigned char *)buffer;
    dest->outbuffer_size = size;
    dest->outbuffer_cursor = (unsigned char *)buffer;
    dest->written = written;
}

static int mjpeg_encode(struct codec_ctx *cc, struct iovec *in, struct iovec *out)
{
    struct mjpeg_ctx *mc = (struct mjpeg_ctx *)cc->priv;
    out->iov_len = mc->width * mc->height * COLOR_COMPONENTS;
    out->iov_base = calloc(1, out->iov_len);
    if (!out->iov_base) {
        loge("malloc mjpeg out buffer failed!\n");
        return -1;
    }
    int quality = DEFAULT_JPEG_QUALITY;
    struct jpeg_compress_struct *encoder = &mc->encoder;

    int i = 0;
    int j = 0;
    int written = 0;

    uint32_t size = mc->width * mc->height;
    uint32_t quarter_size = size / 4;
    uint32_t row = 0;

    JSAMPROW y[16];
    JSAMPROW cb[16];
    JSAMPROW cr[16];
    JSAMPARRAY planes[3];

    planes[0] = y;
    planes[1] = cb;
    planes[2] = cr;

    dest_buffer(encoder, out->iov_base, mc->width * mc->height, &written);

    encoder->image_width = mc->width;	/* image width and height, in pixels */
    encoder->image_height = mc->height;
    encoder->input_components = COLOR_COMPONENTS;	/* # of color components per pixel */
    encoder->in_color_space = JCS_YCbCr;       /* colorspace of input image */

    jpeg_set_defaults(encoder);
    encoder->raw_data_in = TRUE;	// supply downsampled data
    encoder->dct_method = JDCT_IFAST;

    encoder->comp_info[0].h_samp_factor = 2;
    encoder->comp_info[0].v_samp_factor = 2;
    encoder->comp_info[1].h_samp_factor = 1;
    encoder->comp_info[1].v_samp_factor = 1;
    encoder->comp_info[2].h_samp_factor = 1;
    encoder->comp_info[2].v_samp_factor = 1;

    jpeg_set_quality(encoder, quality, TRUE /* limit to baseline-JPEG values */);
    jpeg_start_compress(encoder, TRUE);
    if (mc->input_format == YUV422P) {
        struct iovec in_tmp;
        in_tmp.iov_len = in->iov_len;
        in_tmp.iov_base = calloc(1, in->iov_len);
        conv_yuv422to420p(in_tmp.iov_base, in->iov_base, mc->width, mc->height);
        for (i = 0; i < (int)in->iov_len; i++) {//8 byte copy
            *((char *)in->iov_base + i) = *((char *)in_tmp.iov_base + i);
        }
        free(in_tmp.iov_base);
    }

    for (j = 0; j < mc->height; j += 16) {
        for (i = 0; i < 16; i++) {
            row = mc->width * (i + j);
            y[i] = (uint8_t *)in->iov_base + row;
            if (i % 2 == 0) {
                cb[i/2] = (uint8_t *)in->iov_base + size + row/4;
                cr[i/2] = (uint8_t *)in->iov_base + size + quarter_size + row/4;
            }
        }
        jpeg_write_raw_data(encoder, planes, 16);
    }
    jpeg_finish_compress(encoder);
    out->iov_len = written;
    return 0;
}

static void mjpeg_close(struct codec_ctx *cc)
{
    struct mjpeg_ctx *mc = (struct mjpeg_ctx *)cc->priv;
    jpeg_destroy_compress(&mc->encoder);
    free(mc);
}
struct codec aq_mjpeg_encoder = {
    .name   = "mjpeg",
    .open   = mjpeg_open,
    .encode = mjpeg_encode,
    .decode = NULL,
    .close  = mjpeg_close,
};
