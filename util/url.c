/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    url.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-01 13:57
 * updated: 2016-05-01 13:57
 ******************************************************************************/
#include <string.h>
#include <liblog.h>
#include "url.h"

#define URL_TAG     "://"

int url_parse(struct url *u, const char *input)
{
    char *p;
    int hlen, blen;
    if (!u || !input || strlen(input) <= strlen(URL_TAG)) {
        loge("invalid paraments!\n");
        return -1;
    }

    p = strstr(input, URL_TAG);
    if (!p) {
        loge("input is not url format\n");
        return -1;
    }
    hlen = p - input;
    if (hlen > sizeof(u->head)) {
        loge("url head size is larger than %d\n", sizeof(u->head));
        return -1;
    }
    p += strlen(URL_TAG);
    blen = input + strlen(input) - p;
    if (blen > sizeof(u->body)) {
        loge("url body size is larger than %d\n", sizeof(u->body));
        return -1;
    }
    strncpy(u->head, input, hlen);
    strncpy(u->body, p, blen);
    return 0;
}

