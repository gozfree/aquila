/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    imgconvert.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-16 00:27
 * updated: 2016-05-16 00:27
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
void conv_yuv420to422p(void *yuv420, void *yuv422, int width, int height, int len);
void conv_uyvyto420p(void *dst, void *src, int width, int height);
void conv_rgb24toyuv420p(void *dst, void *src, int width, int height);


#ifdef __cplusplus
}
#endif
#endif
