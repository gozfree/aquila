/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    common.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-02 16:07
 * updated: 2016-05-02 16:07
 ******************************************************************************/
#ifndef _COMMON_H_
#define _COMMON_H_

#include <stdint.h>
#include <stdlib.h>
#include <sys/uio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct frame {
    struct iovec buf;
    int index;
};


struct media_params {
    struct {
        int width;
        int height;
        int pix_fmt;
    } video;
    struct {
    } audio;
};

#ifdef __cplusplus
}
#endif
#endif
