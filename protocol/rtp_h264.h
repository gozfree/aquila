/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    rtp_h264.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-24 22:47
 * updated: 2016-05-24 22:47
 ******************************************************************************/
#ifndef _RTP_H264_H_
#define _RTP_H264_H_

#ifdef __cplusplus
extern "C" {
#endif


int h264_find_nalu(void *buf, size_t len);

#ifdef __cplusplus
}
#endif
#endif
