/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    url.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-04-30 17:27
 * updated: 2016-04-30 17:27
 ******************************************************************************/
#ifndef _URL_H_
#define _URL_H_

#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


/*
 *TODO:
 * authorization (user[:pass]@hostname)
 *
 *
 *
 */

// url = rtsp://192.168.1.1:8554/1.264
// head = rtsp
// body = 192.168.1.1:8554/1.264
struct url {
    char head[32];
    char body[512];
};

int url_parse(struct url *u, const char *input);


#ifdef __cplusplus
}
#endif
#endif
