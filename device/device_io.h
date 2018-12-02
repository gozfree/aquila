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
#ifndef _DEVICE_IO_H_
#define _DEVICE_IO_H_

#include <linux/ioctl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct video_cap {
    uint8_t desc[32];
    uint32_t version;
};

struct video_ctrl {
    uint32_t cmd;
    uint32_t val;
};

#define DEV_VIDEO_GET_CAP  _IOWR('V',  0, struct video_cap)
#define DEV_VIDEO_SET_CTRL _IOWR('V',  1, struct video_ctrl)


#ifdef __cplusplus
}
#endif
#endif
