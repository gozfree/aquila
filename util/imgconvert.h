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
#ifndef _IMGCONVERT_H_
#define _IMGCONVERT_H_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


void conv_yuv422to420p(void *dst, void *src, int width, int height);
void conv_yuv420pto422(void *dst, void *src, int width, int height);
void conv_yuv420to422p(void *yuv420, void *yuv422, int width, int height, int len);
void conv_uyvyto420p(void *dst, void *src, int width, int height);
void conv_rgb24toyuv420p(void *dst, void *src, int width, int height);


#ifdef __cplusplus
}
#endif
#endif
