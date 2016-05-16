/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    config.h
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-16 11:27
 * updated: 2016-05-16 11:27
 ******************************************************************************/
#ifndef _CONFIG_H_
#define _CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

struct aq_config {
    struct {
        char type[32];
        int width;
        int height;
    } videocap;

    struct {
        char type[32];
    } vencode;

    struct {
        char type[32];
    } playback;

    struct {
        char source[32];
        char sink[32];
    } filter[4];

};

int load_conf();
void unload_conf();


#ifdef __cplusplus
}
#endif
#endif
