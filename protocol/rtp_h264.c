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
#include <stdint.h>
#include <string.h>
#include <libmacro.h>
#include <liblog.h>
#include <libvector.h>

/* Generic Nal header
 *  +---------------+
 *  |7|6|5|4|3|2|1|0|
 *  +-+-+-+-+-+-+-+-+
 *  |F|NRI|  Type   |
 *  +---------------+
 */

/*  FU header
 *  +---------------+
 *  |7|6|5|4|3|2|1|0|
 *  +-+-+-+-+-+-+-+-+
 *  |S|E|R|  Type   |
 *  +---------------+
 */

/* NAL unit types */
typedef enum h264_nalu_type {
    NAL_SLICE           = 1,
    NAL_DPA             = 2,
    NAL_DPB             = 3,
    NAL_DPC             = 4,
    NAL_IDR_SLICE       = 5,
    NAL_SEI             = 6,
    NAL_SPS             = 7,
    NAL_PPS             = 8,
    NAL_AUD             = 9,
    NAL_END_SEQUENCE    = 10,
    NAL_END_STREAM      = 11,
    NAL_FILLER_DATA     = 12,
    NAL_SPS_EXT         = 13,
    NAL_AUXILIARY_SLICE = 19,
    NAL_FF_IGNORE       = 0xff0f001,
} h264_nalu_type_t;

typedef struct h264_nalu {
    uint8_t *addr;
    size_t size;
    h264_nalu_type_t type;
} h264_nalu_t;

typedef struct h264_FU_header {
    uint8_t type :5; /* Bit 0 ~ 4 */
    uint8_t r    :1; /* Bit 5     */
    uint8_t e    :1; /* Bit 6     */
    uint8_t s    :1; /* Bit 7     */
} h264_FU_header_t;

char *base64_encode(const uint8_t *data, uint32_t data_len)
{
    if ((!data || data_len == 0)) {
        return NULL;
    }
    const char ptn[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const uint32_t numOrig24BitValues = data_len / 3;
    int havePadding  = (data_len > (numOrig24BitValues * 3));
    int havePadding2 = (data_len == (numOrig24BitValues * 3 + 2));
    const uint32_t bytes = 4 * (numOrig24BitValues + (havePadding ? 1 : 0));
    char *str = CALLOC(bytes + 1, char);
    uint32_t i = 0;

    /* Map each full group of 3 input bytes into 4 output base-64 characters: */
    for (i = 0; i < numOrig24BitValues; ++ i) {
        str[4 * i + 0] = ptn[(data[3 * i] >> 2) & 0x3F];
        str[4 * i + 1] = ptn[(((data[3 * i] & 0x3) << 4) |
                        (data[3 * i + 1] >> 4)) & 0x3F];
        str[4 * i + 2] = ptn[((data[3 * i + 1] << 2) |
                        (data[3 * i + 2] >> 6)) & 0x3F];
        str[4 * i + 3] = ptn[data[3 * i + 2] & 0x3F];
    }

    /* Now, take padding into account.  (Note: i == numOrig24BitValues) */
    if (havePadding) {
        str[4 * i + 0] = ptn[(data[ 3 * i] >> 2) & 0x3F];
        if (havePadding2) {
            str[4 * i + 1] = ptn[(((data[3 * i] & 0x3) << 4) |
                            (data[3 * i + 1] >> 4)) & 0x3F];
            str[4 * i + 2] = ptn[(data[3 * i + 1] << 2) & 0x3F];
        } else {
            str[4 * i + 1] = ptn[((data[3 * i] & 0x3) << 4) & 0x3F];
            str[4 * i + 2] = '=';
        }
        str[4 * i + 3] = '=';
    }
    str[bytes] = '\0';
    return str;
}


extern "C" {
int h264_find_nalu(void *buf, size_t len)
{
    struct vector *nalu_list;
    nalu_list = vector_create(struct h264_nalu);
    uint8_t *bs = (uint8_t *)buf;
    //char *m_sps;
    //char *m_pps;
    uint8_t *end = bs + len - 4;
    struct h264_nalu nalu;
    struct h264_nalu *prev, *curr, *tmp;
    void *vtmp;
    int index = -1;
    if (len < 4) {
        loge("buf is too short, no need to find nalu\n");
        return -1;
    }
    logi("start find nalu\n");
    while (bs < end) {
        if (0x00000001 == (bs[0]<<24|bs[1]<<16|bs[2]<<8|bs[3])) {
            logi("find nalu\n");
            ++index;
            bs += 4;
            nalu.type = (h264_nalu_type_t)(bs[0]&0x1F);
            nalu.addr = bs;
            vector_push_back(nalu_list, nalu);
            tmp = vector_back(nalu_list, struct h264_nalu);
            logi("index = %d, nalu.type = %d, addr = %p\n", index, tmp->type, tmp->addr);
            if (index > 0) {
                prev = vector_at(nalu_list, index - 1, struct h264_nalu);
                //vtmp = get_member(nalu_list, index - 1);
                VERBOSE();
                logi("index = %d, prev = %x, vtmp = %x\n", index, prev, vtmp);
                logi("prev_nalu = %p\n", nalu_list->buf.iov_base);
                curr = vector_at(nalu_list, index, struct h264_nalu);
                /* calculate the previous NALU's size */
                VERBOSE();
                logi("prev_nalu.addr = %p\n", prev->addr);
                prev->size = curr->addr - prev->addr - 4;
                switch(prev->type) {
                case NAL_SPS:
                    //m_sps = base64_encode(prev->addr, prev->size);
                    /*uint32_t m_profile_level_id = (prev->addr[1] << 16) |
                            (prev->addr[2] <<  8) |
                            prev->addr[3];*/
                    break;
                case NAL_PPS:
                    //m_pps = base64_encode(prev->addr, prev->size);
                    break;
                default:
                    break;
                }
            }
        } else if (bs[3] != 0) {
            logd("not find nalu 1\n");
            bs += 4;
        } else if (bs[2] != 0) {
            logd("not find nalu 2\n");
            bs += 3;
        } else if (bs[1] != 0) {
            logd("not find nalu 3\n");
            bs += 2;
        } else {
            logd("not find nalu 4\n");
            bs += 1;
        }
    }
    logi("finished find nalu\n");
    return 0;
}
}
