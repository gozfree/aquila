/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    imgconvert.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-08 23:14
 * updated: 2016-05-08 23:14
 ******************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include "imgconvert.h"

void conv_yuv422to420p(void *yuv420p, void *yuv422, int width, int height)
{
    uint8_t *src, *dst, *src2, *dst2;
    int i, j;

    /* Create the Y plane. */
    src = (uint8_t *)yuv422;
    dst = (uint8_t *)yuv420p;
    for (i = width * height; i > 0; i--) {
        *dst = *src;
        dst += 1;
        src += 2;
    }
    /* Create U and V planes. */
    src = (uint8_t *)yuv422 + 1;
    src2 = (uint8_t *)yuv422 + width * 2 + 1;
    dst = (uint8_t *)yuv420p + width * height;
    dst2 = dst + (width * height) / 4;
    for (i = height / 2; i > 0; i--) {
        for (j = width / 2; j > 0; j--) {
            //Copy U
            *dst = ((int) *src + (int) *src2) / 2;
            src += 2;
            src2 += 2;
            dst++;
            //Copy V
            *dst2 = ((int) *src + (int) *src2) / 2;
            src += 2;
            src2 += 2;
            dst2++;
        }
        src += width * 2;
        src2 += width * 2;
    }
}

void conv_yuv420pto422(void *yuv422, void *yuv420p, int width, int height)
{
    uint8_t *src, *dst, *src2, *dst2, *xxx, *xxx2;
    int i, j;

    /* Create the Y plane. */
    src = (uint8_t *)yuv420p;
    dst = (uint8_t *)yuv422;
    for (i = width * height; i > 0; i--) {
        *dst = *src;
        dst += 2;
        src += 1;
    }
    /* Create U and V planes. */
    src = (uint8_t *)yuv420p + width * height;
    src2 = src + (width * height) / 4;
    dst = (uint8_t *)yuv422 + 1;//u
    dst2 = (uint8_t *)dst + width * 2 + 1;
    xxx = (uint8_t *)yuv422 + 3;//v
    xxx2 = (uint8_t *)xxx + width * 2 + 1;
    for (i = height / 2; i > 0; i--) {
        for (j = width / 2; j > 0; j--) {
            //Copy U
            *dst = *src;
            *dst2 = *src;
            src += 1;
            dst += 4;
            dst2 += 4;
            //Copy V
            *xxx = *src2;
            *xxx2 = *src2;
            src2 += 1;
            xxx += 4;
            xxx2 += 4;
        }
        dst += width * 2;
        dst2 += width * 2;
        xxx += width * 2;
        xxx2 += width * 2;
    }
}


void conv_uyvyto420p(void *dst, void *src, int width, int height)
{
    uint8_t *pY = (uint8_t *)dst;
    uint8_t *pU = pY + (width * height);
    uint8_t *pV = pU + (width * height) / 4;
    uint32_t uv_offset = width * 2 * sizeof(uint8_t);
    uint32_t ix, jx;

    for (ix = 0; ix < (uint32_t)height; ix++) {
        for (jx = 0; jx < (uint32_t)width; jx += 2) {
            uint16_t calc;

            if ((ix&1) == 0) {
                calc = *(uint16_t *)src;
                calc += *((uint8_t *)src + uv_offset);
                calc /= 2;
                *pU++ = (uint8_t) calc;
            }

            src = ((uint8_t *)src) + 1;
            *pY++ = (*(uint8_t *)src)++;

            if ((ix&1) == 0) {
                calc = *(uint16_t *)src;
                calc += *((uint8_t *)src + uv_offset);
                calc /= 2;
                *pV++ = (uint8_t) calc;
            }

            src = ((uint8_t *)src) + 1;
            *pY++ = (*(uint8_t *)src)++;
        }
    }
}

void conv_rgb24toyuv420p(void *dst, void *src, int width, int height)
{
    uint8_t *y, *u, *v;
    uint8_t *r, *g, *b;
    int i, loop;

    b = (uint8_t *)src;
    g = b + 1;
    r = g + 1;
    y = (uint8_t *)dst;
    u = y + width * height;
    v = u + (width * height) / 4;
    memset(u, 0, width * height / 4);
    memset(v, 0, width * height / 4);

    for (loop = 0; loop < height; loop++) {
        for (i = 0; i < width; i += 2) {
            *y++ = (9796 ** r + 19235 ** g + 3736 ** b) >> 15;
            *u += ((-4784 ** r - 9437 ** g + 14221 ** b) >> 17) + 32;
            *v += ((20218 ** r - 16941 ** g - 3277 ** b) >> 17) + 32;
            r += 3;
            g += 3;
            b += 3;
            *y++ = (9796 ** r + 19235 ** g + 3736 ** b) >> 15;
            *u += ((-4784 ** r - 9437 ** g + 14221 ** b) >> 17) + 32;
            *v += ((20218 ** r - 16941 ** g - 3277 ** b) >> 17) + 32;
            r += 3;
            g += 3;
            b += 3;
            u++;
            v++;
        }

        if ((loop & 1) == 0) {
            u -= width / 2;
            v -= width / 2;
        }
    }
}
