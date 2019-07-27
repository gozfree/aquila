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
#include <string.h>
#include <liblog.h>
#include "url.h"

#define URL_TAG     "://"

int url_parse(struct url *u, const char *input)
{
    char *p;
    size_t hlen, blen;
    if (!u || !input || strlen(input) <= strlen(URL_TAG)) {
        loge("invalid paraments %s!\n", input);
        return -1;
    }

    p = (char *)strstr(input, URL_TAG);
    if (!p) {
        loge("input %s is not url format\n", input);
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

