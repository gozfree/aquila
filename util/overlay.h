/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    overlay.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-21 14:57
 * updated: 2016-05-21 14:57
 ******************************************************************************/
#ifndef _OVERLAY_H_
#define _OVERLAY_H_
#ifdef __cplusplus
extern "C" {
#endif

int overlay_init();
int overlay_draw_text(unsigned char *image, unsigned int startx, unsigned int starty, unsigned int width, const char *text);

#ifdef __cplusplus
}
#endif
#endif
