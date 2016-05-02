/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    v4l2.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-04-21 22:49
 * updated: 2016-04-21 22:49
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <libgzf.h>
#include <liblog.h>
#include "device.h"
#include "common.h"

#define MAX_V4L_BUF         (32)
#define MAX_V4L_REQBUF_CNT  (256)

struct v4l2_ctx {
    int fd;
    int on_read_fd;
    int on_write_fd;
    int width;
    int height;
    struct iovec buf[MAX_V4L_BUF];
    int buf_index;
    int req_count;
};

static int v4l2_set_format(struct v4l2_ctx *vc)
{
    struct v4l2_fmtdesc fmtdesc;
    struct v4l2_format fmt;
    struct v4l2_pix_format *pix = &fmt.fmt.pix;

    fmtdesc.index = 0;
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    logd("V4L2 Support Format:\n");
    while (-1 != ioctl(vc->fd,VIDIOC_ENUM_FMT,&fmtdesc)) {
        logd("\t%d. %s\n", fmtdesc.index, fmtdesc.description);
        ++fmtdesc.index;
    }

    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(vc->fd, VIDIOC_G_FMT, &fmt)) {
        loge("ioctl(VIDIOC_G_FMT) failed(%d): %s\n", errno, strerror(errno));
        return -1;
    }
    logd("v4l2 default pix format: %d*%d\n", pix->width, pix->height);

    if (vc->width > 0 || vc->height > 0) {
        //set v4l2 pixel format
        pix->width = vc->width;
        pix->height = vc->height;
    } else {
        vc->width = pix->width;
        vc->height = pix->height;
    }
    logd("v4l2 using pix format: %d*%d\n", pix->width, pix->height);
    if (-1 == ioctl(vc->fd, VIDIOC_S_FMT, &fmt)) {
        loge("ioctl(VIDIOC_S_FMT) failed(%d): %s\n", errno, strerror(errno));
        return -1;
    }

    return 0;
}

static int v4l2_buf_enqueue(struct v4l2_ctx *vc)
{
    struct v4l2_buffer buf;
    char notify = '1';
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = vc->buf_index;

    if (-1 == ioctl(vc->fd, VIDIOC_QBUF, &buf)) {
        loge("ioctl(VIDIOC_QBUF) failed(%d): %s\n", errno, strerror(errno));
        return -1;
    }
    if (write(vc->on_write_fd, &notify, 1) != 1) {
        loge("Failed writing to notify pipe: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

static int v4l2_req_buf(struct v4l2_ctx *vc)
{
    int i;
    struct v4l2_requestbuffers req;
    enum v4l2_buf_type type;
    char notify = '1';

    memset(&req, 0, sizeof(req));
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.count = MAX_V4L_REQBUF_CNT;
    req.memory = V4L2_MEMORY_MMAP;
    //request buffer
    if (-1 == ioctl(vc->fd, VIDIOC_REQBUFS, &req)) {
        loge("ioctl(VIDIOC_REQBUFS) failed(%d): %s\n", errno, strerror(errno));
        return -1;
    }
    vc->req_count = req.count;
    if (req.count > MAX_V4L_REQBUF_CNT || req.count < 2) {
        loge("Insufficient buffer memory\n");
        return -1;
    }
    for (i = 0; i < req.count; i++) {
        struct v4l2_buffer buf;
        memset(&buf, 0, sizeof(buf));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index  = i;
        //query buffer
        if (-1 == ioctl(vc->fd, VIDIOC_QUERYBUF, &buf)) {
            loge("ioctl(VIDIOC_QUERYBUF) failed(%d): %s\n",
                 errno, strerror(errno));
            return -1;
        }

        //mmap buffer
        vc->buf[i].iov_len = buf.length;
        vc->buf[i].iov_base = mmap(NULL, buf.length, PROT_READ|PROT_WRITE,
                               MAP_SHARED, vc->fd, buf.m.offset);
        if (MAP_FAILED == vc->buf[i].iov_base) {
            loge("mmap failed: %s\n", strerror(errno));
            return -1;
        }

        //enqueue buffer
        if (-1 == ioctl(vc->fd, VIDIOC_QBUF, &buf)) {
            loge("ioctl(VIDIOC_QBUF) failed(%d): %s\n", errno, strerror(errno));
            return -1;
        }
    }

    //stream on
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (-1 == ioctl(vc->fd, VIDIOC_STREAMON, &type)) {
        loge("ioctl(VIDIOC_STREAMON) failed(%d): %s\n", errno, strerror(errno));
        return -1;
    }
    if (write(vc->on_write_fd, &notify, 1) != 1) {
        loge("Failed writing to notify pipe\n");
        return -1;
    }
    return 0;
}

static int v4l2_buf_dequeue(struct v4l2_ctx *vc, struct frame *f)
{
    struct v4l2_buffer buf;
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    while (1) {
        if (-1 == ioctl(vc->fd, VIDIOC_DQBUF, &buf)) {
            loge("ioctl(VIDIOC_DQBUF) failed(%d): %s\n",
                 errno, strerror(errno));
            if (errno == EINTR || errno == EAGAIN)
                continue;
            else
                return -1;
        }
        break;
    }
    vc->buf_index = buf.index;
    f->index = buf.index;
    f->buf.iov_base = vc->buf[buf.index].iov_base;
    f->buf.iov_len = buf.bytesused;
//    loge("v4l2 frame buffer addr = %p, len = %d, index = %d\n",
//    f->addr, f->len, f->index);

    return f->buf.iov_len;
}

static int v4l2_open(struct device_ctx *dc, const char *dev)
{
    int fd = -1;
    int fds[2] = {0};
    struct v4l2_ctx *vc = CALLOC(1, struct v4l2_ctx);
    if (!vc) {
        loge("malloc v4l2_ctx failed!\n");
        return -1;
    }
    if (pipe(fds)) {
        loge("create pipe failed(%d): %s\n", errno, strerror(errno));
        goto failed;
    }
    vc->on_read_fd = fds[0];
    vc->on_write_fd = fds[1];
    logd("pipe: rfd = %d, wfd = %d\n", vc->on_read_fd, vc->on_write_fd);

    fd = open(dev, O_RDWR);
    if (fd == -1) {
        loge("open %s failed: %s\n", dev, strerror(errno));
        goto failed;
    }
    vc->fd = fd;
    vc->width = 640;//XXX
    vc->height = 480;//XXX

    if (-1 == v4l2_set_format(vc)) {
        loge("v4l2_set_format failed\n");
        goto failed;
    }
    if (-1 == v4l2_req_buf(vc)) {
        loge("v4l2_req_buf failed\n");
        goto failed;
    }

    dc->fd = vc->on_read_fd;//use pipe fd to trigger event
    dc->media.video.width = vc->width;
    dc->media.video.height = vc->height;
    dc->priv = vc;
    return 0;

failed:
    if (fd != -1) {
        close(fd);
    }
    if (vc) {
        free(vc);
    }
    if (fds[0] != -1 || fds[1] != -1) {
        close(fds[0]);
        close(fds[1]);
    }
    return -1;
}

static int v4l2_read(struct device_ctx *dc, void *buf, int len)
{
    struct v4l2_ctx *vc = (struct v4l2_ctx *)dc->priv;
    struct frame f;
    int i, flen;
    char notify;

    if (read(vc->on_read_fd, &notify, sizeof(notify)) != 1) {
        perror("Failed read from notify pipe");
    }

    flen = v4l2_buf_dequeue(vc, &f);
    if (flen == -1) {
        loge("v4l2 dequeue failed!\n");
        return -1;
    }
    if (flen > len) {
        loge("v4l2 frame is %d bytes, but buffer len is %d, not enough!\n",
             flen, len);
        return -1;
    }
    if (len < f.buf.iov_len) {
        loge("error occur!\n");
        return -1;
    }

    for (i = 0; i < f.buf.iov_len; i++) {//8 byte copy
        *((char *)buf + i) = *((char *)f.buf.iov_base + i);
    }
    return f.buf.iov_len;
}

static int v4l2_write(struct device_ctx *dc, void *buf, int len)
{
    struct v4l2_ctx *vc = (struct v4l2_ctx *)dc->priv;
    return v4l2_buf_enqueue(vc);
}

static void v4l2_close(struct device_ctx *dc)
{
    int i;
    struct v4l2_ctx *vc = (struct v4l2_ctx *)dc->priv;
    enum v4l2_buf_type type;
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == ioctl(vc->fd, VIDIOC_STREAMOFF, &type)) {
        loge("ioctl(VIDIOC_STREAMOFF) failed(%d): %s\n",
             errno, strerror(errno));
    }
    for (i = 0; i < vc->req_count; i++) {
        munmap(vc->buf[i].iov_base, vc->buf[i].iov_len);
    }
    close(vc->fd);
    close(vc->on_read_fd);
    close(vc->on_write_fd);
    free(vc);
}

struct device aq_v4l2_device = {
    .name = "v4l2",
    .open = v4l2_open,
    .read = v4l2_read,
    .write = v4l2_write,
    .close = v4l2_close,
};
