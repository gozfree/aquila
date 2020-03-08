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
#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>
#include <gear-lib/libmedia-io.h>

#ifdef __cplusplus
extern "C" {
#endif

enum conf_map_index {
    ALSA = 0,
    MJPEG,
    PLAYBACK,
    SDL,
    SNKFAKE,
    UPSTREAM,
    USBCAM,
    VDEVFAKE,
    VENCODE,
    VDECODE,
    VIDEOCAP,
    RECORD,
    REMOTECTRL,
    X264,
    H264ENC,
    YUV420,
    YUV422P,
    UNKNOWN = -1
};

enum yuv_format {
    YUV420_IYUV = 0,	// YYYYYYYYUUVV
    YUV420_YV12 = 1,	// YYYYYYYYVVUU
    YUV420_NV12 = 2,	// YYYYYYYYUVUV
    YUV422_YU16 = 3,	// YYYYYYYYUUUUVVVV
    YUV422_YV16 = 4,	// YYYYYYYYVVVVUUUU
    YUV422_YUYV = 5,	// YUYVYUYVYUYVYUYV
};

struct ikey_cvalue {
    int val;
    char str[32];
};


#ifdef __cplusplus
}
#endif
#endif
