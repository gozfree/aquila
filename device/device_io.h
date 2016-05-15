/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    device_io.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-15 21:23
 * updated: 2016-05-15 21:23
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
