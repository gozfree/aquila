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
#include "protocol.h"
#include "device.h"
#include <gear-lib/librpc.h>
#include <gear-lib/librpc_stub.h>
#include <gear-lib/libgevent.h>
#include <gear-lib/liblog.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define RPCD_PORT           (54321)

static int on_shell_help(struct rpc_session *r, void *ibuf, size_t ilen, void **obuf, size_t *olen)
{
    int ret;
    char *cmd = (char *)ibuf;
    *obuf = calloc(1, 1024);
    *olen = 1024;
    printf("on_shell_help cmd = %s\n", cmd);
    memset(*obuf, 0, 1024);
    ret = 0;//system_with_result(cmd, *obuf, 1024);
    if (ret > 0) {
        *olen = ret;
    }
    //device_ioctl(NULL, 0, NULL, 0);
    return 0;
}

BEGIN_RPC_MAP(BASIC_RPC_API)
RPC_MAP(RPC_SHELL_HELP, on_shell_help)
END_RPC_MAP()

int rpcd_group_register()
{
    RPC_REGISTER_MSG_MAP(BASIC_RPC_API);
    return 0;
}


static int rpcd_open(struct protocol_ctx *pc, const char *url, struct media_encoder *media, void *conf)
{
    int port = RPCD_PORT;
    struct rpcs *rpc = rpc_server_create(NULL, port);
    loge("rpcd created!\n");
    rpcd_group_register();
    pc->priv = rpc;
    return 0;
}

static int rpcd_read(struct protocol_ctx *pc, void *buf, int len)
{
    return 0;
}

static int rpcd_write(struct protocol_ctx *pc, void *buf, int len)
{
    return 0;
}

static void rpcd_close(struct protocol_ctx *pc)
{
    struct rpcs *rpc = (struct rpcs *)pc->priv;
    rpc_server_destroy(rpc);
}

struct protocol aq_rpcd_protocol = {
    .name = "rpcd",
    .open = rpcd_open,
    .read = rpcd_read,
    .write = rpcd_write,
    .close = rpcd_close,
};
