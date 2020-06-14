/******************************************************************************
 * Copyright (C) 2014-2020 Zhifeng Gong <gozfree@163.com>
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
#ifndef _AQUILA_H_
#define _AQUILA_H_

#include <gear-lib/libdarray.h>
#include <gear-lib/libqueue.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "filter.h"
#include "config.h"

struct aquila {
    struct aq_config config;
    struct queue **queue;
    struct filter_ctx **filter;
    bool run;
};


#endif
