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
#include <librpc.h>
#include <librpc_stub.h>
#include <libgevent.h>
#include <libhash.h>
#include <libworkq.h>
#include <liblog.h>
#include <libskt.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define RPCD_PORT           (54321)
#define MAX_UUID_LEN        (21)

struct rpcd {
    int listen_fd;
    struct gevent_base *evbase;
    struct hash *dict_uuid2fd;
    struct hash *dict_fd2rpc;
    struct workq *wq;
};


struct wq_arg {
    msg_handler_t handler;
    struct rpc r;
    void *buf;
    size_t len;

};


static int on_shell_help(struct rpc *r, void *arg, int len)
{
    int ret;
    char buf[1024];
    char *cmd = (char *)arg;
    logi("on_shell_help cmd = %s\n", cmd);
    memset(buf, 0, sizeof(buf));
    ret = 0;//system_with_result(cmd, buf, sizeof(buf));
    loge("ret = %d, errno = %d\n", ret, errno);
    logi("send len = %d, buf: %s\n", strlen(buf), buf);
    rpc_send(r, buf, strlen(buf));
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
static int create_uuid(char *uuid, int len, int fd, uint32_t ip, uint16_t port)
{
    snprintf(uuid, MAX_UUID_LEN, "%08x%08x%04x", fd, ip, port);
    return 0;
}

static void on_error(int fd, void *arg)
{
    loge("error: %d\n", errno);
}

static int rpcd_connect_del(struct rpcd *rpcd, int fd, uint32_t uuid)
{
    char uuid_str[9];
    char fd_str[9];
    snprintf(fd_str, sizeof(fd_str), "%08x", fd);
    snprintf(uuid_str, sizeof(uuid_str), "%08x", uuid);
    hash_del(rpcd->dict_fd2rpc, fd_str);
    hash_del(rpcd->dict_uuid2fd, uuid_str);
    logd("delete connection fd:%s, uuid:%s\n", fd_str, uuid_str);
    return 0;
}

static int rpcd_connect_add(struct rpcd *rc, struct rpc *r, int fd, uint32_t uuid)
{
    char fd_str[9];
    char uuid_str[9];
    char *fdval = (char *)calloc(1, 9);
    snprintf(fd_str, sizeof(fd_str), "%08x", fd);
    snprintf(uuid_str, sizeof(uuid_str), "%08x", uuid);
    snprintf(fdval, 9, "%08x", fd);
    hash_set(rc->dict_fd2rpc, fd_str, (char *)r);
    hash_set(rc->dict_uuid2fd, uuid_str, fdval);
    logd("add connection fd:%s, uuid:%s\n", fd_str, uuid_str);
    return 0;
}


static void rpc_connect_destroy(struct rpcd *rpcd, struct rpc *r)
{
    if (!rpcd || !r) {
        loge("invalid paramets!\n");
        return;
    }
    int fd = r->fd;
    uint32_t uuid = r->send_pkt.header.uuid_src;
    struct gevent *e = r->ev;
    rpcd_connect_del(rpcd, fd, uuid);
    gevent_del(rpcd->evbase, e);
}

static void process_wq(void *arg)
{
    struct wq_arg *wq = (struct wq_arg *)arg;
    if (&wq->handler) {
        wq->handler.cb(&wq->r, wq->buf, wq->len);
    }
}

static int do_process_msg(struct rpc *r, void *buf, size_t len)
{
    char uuid_str[4];
    int ret;
    msg_handler_t *msg_handler;
    struct rpcd *rc = (struct rpcd *)r->opaque;
    struct rpc_header *h = &r->recv_pkt.header;
    int msg_id = rpc_packet_parse(r);
    logi("msg_id = %08x\n", msg_id);

    msg_handler = find_msg_handler(msg_id);
    if (msg_handler) {
        struct wq_arg *arg = CALLOC(1, struct wq_arg);
        memcpy(&arg->handler, msg_handler, sizeof(msg_handler_t));
        memcpy(&arg->r, r, sizeof(struct rpc));
        arg->buf = calloc(1, len);
        memcpy(arg->buf, buf, len);
        arg->len = len;
        wq_task_add(rc->wq, process_wq, arg, sizeof(struct wq_arg));
        //msg_handler->cb(r, buf, len);
    } else {
        loge("no callback for this MSG ID(%d) in process_msg\n", msg_id);
        snprintf(uuid_str, sizeof(uuid_str), "%x", h->uuid_dst);
        char *valfd = (char *)hash_get(rc->dict_uuid2fd, uuid_str);
        if (!valfd) {
            loge("hash_get failed: key=%s\n", h->uuid_dst);
            return -1;
        }
        int dst_fd = strtol(valfd, NULL, 16);
        r->fd = dst_fd;
        ret = rpc_send(r, buf, len);
    }
    return ret;
}

static void on_recv(int fd, void *arg)
{
    struct iovec *buf;
    char key[9];
    struct rpcd *rc = (struct rpcd *)(((struct rpc *)arg)->opaque);
    snprintf(key, sizeof(key), "%08x", fd);
    struct rpc *r = (struct rpc *)hash_get(rc->dict_fd2rpc, key);
    if (!r) {
        loge("hash_get failed: key=%s", key);
        return;
    }
    buf = rpc_recv_buf(r);
    if (!buf) {
        logd("on_disconnect fd = %d\n", r->fd);
        rpc_connect_destroy(rc, r);
        return;
    }
    do_process_msg(r, buf->iov_base, buf->iov_len);
    r->fd = fd;//must be reset
    free(buf->iov_base);
    free(buf);
}

static struct rpc *rpc_connect_create(struct rpcd *rc,
                int fd, uint32_t ip, uint16_t port)
{
    char str_ip[INET_ADDRSTRLEN];
    char uuid[MAX_UUID_LEN];
    uint32_t uuid_hash;
    int ret;

    struct rpc *r = (struct rpc *)calloc(1, sizeof(struct rpc));
    if (!r) {
        loge("malloc failed!\n");
        return NULL;
    }
    r->fd = fd;
    r->opaque = rc;
    create_uuid(uuid, MAX_UUID_LEN, fd, ip, port);
    uuid_hash = hash_gen32(uuid, sizeof(uuid));
    struct gevent *e = gevent_create(fd, on_recv, NULL, on_error, (void *)r);
    if (-1 == gevent_add(rc->evbase, e)) {
        loge("event_add failed!\n");
    }
    r->ev = e;

    r->send_pkt.header.uuid_src = uuid_hash;
    r->send_pkt.header.uuid_dst = uuid_hash;
    r->send_pkt.header.msg_id = 0;
    r->send_pkt.header.payload_len = sizeof(uuid_hash);
    r->send_pkt.payload = &uuid_hash;
    ret = rpc_send(r, r->send_pkt.payload, r->send_pkt.header.payload_len);
    if (ret == -1) {
        loge("rpc_send failed\n");
    }
    rpcd_connect_add(rc, r, fd, uuid_hash);
    skt_addr_ntop(str_ip, ip);
    logd("on_connect fd = %d, remote_addr = %s:%d, uuid=0x%08x\n",
                    fd, str_ip, port, uuid_hash);

    return r;
}

static void on_connect(int fd, void *arg)
{
    int afd;
    uint32_t ip;
    uint16_t port;
    struct rpcd *rc = (struct rpcd *)arg;

    afd = skt_accept(fd, &ip, &port);
    if (afd == -1) {
        loge("skt_accept failed: %d\n", errno);
        return;
    }
    rpc_connect_create(rc, afd, ip, port);
}

static int rpcd_open(struct protocol_ctx *pc, const char *url, struct media_params *media)
{
    int fd;
    int port = RPCD_PORT;
    struct rpcd *rc = CALLOC(1, struct rpcd);
    if (!rc) {
        loge("malloc rpcd failed!\n");
        return -1;
    }

    fd = skt_tcp_bind_listen(NULL, port);
    if (fd == -1) {
        loge("skt_tcp_bind_listen port:%d failed!\n", port);
        return -1;
    }
    if (0 > skt_set_tcp_keepalive(fd, 1)) {
        loge("skt_set_tcp_keepalive failed!\n");
        return -1;
    }
    rc->listen_fd = fd;
    rc->evbase = gevent_base_create();
    if (!rc->evbase) {
        loge("gevent_base_create failed!\n");
        return -1;
    }
    struct gevent *e = gevent_create(fd, on_connect, NULL, on_error,
                    (void *)rc);
    if (-1 == gevent_add(rc->evbase, e)) {
        loge("event_add failed!\n");
        gevent_destroy(e);
    }
    rc->dict_fd2rpc = hash_create(10240);
    rc->dict_uuid2fd = hash_create(10240);
    rc->wq = wq_create();
    pc->priv = rc;
    loge("rpcd created!\n");
    rpcd_group_register();
    gevent_base_loop_start(rc->evbase);
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
    struct rpcd *rc = (struct rpcd *)pc->priv;
    gevent_base_loop_stop(rc->evbase);
    gevent_base_destroy(rc->evbase);

}

struct protocol aq_rpcd_protocol = {
    .name = "rpcd",
    .open = rpcd_open,
    .read = rpcd_read,
    .write = rpcd_write,
    .close = rpcd_close,
};
