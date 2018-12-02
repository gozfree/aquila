/******************************************************************************
 * Copyright (C) 2014-2015
 * file:    sdl.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-02 19:18
 * updated: 2016-05-02 19:18
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>
#include <SDL_thread.h>
#include <libavformat/avformat.h>
#include <libmacro.h>
#include <liblog.h>

#include "playback.h"
#include "common.h"


const char *wnd_title = "Aquila Media Player v0.1";

enum wnd_type {
    DATA_TYPE_RGB,
    DATA_TYPE_YUV
};

struct sdl_rgb_ctx {
    SDL_Surface *surface;
    uint32_t rmask;
    uint32_t gmask;
    uint32_t bmask;
    uint32_t amask;
    int width;
    int height;
    int bpp;
    int pitch;
    uint8_t *pixels;
    int pixels_num;
};

struct sdl_yuv_ctx {
    SDL_Overlay *overlay;
};

struct sdl_ctx {
    int index;
    int width;
    int height;
    int bpp;
    int type;
    struct sdl_rgb_ctx *rgb;
    struct sdl_yuv_ctx *yuv;
    SDL_Surface *surface;
    SDL_Event event;
    SDL_Thread *event_thread;

    SDL_mutex *mutex;
    SDL_cond *init_cond;
};

static int clamp(double x)
{
    int r = x;
    if (r < 0)
        return 0;
    else if (r > 255)
        return 255;
    else
        return r;
}

static void yuv2rgb(uint8_t Y, uint8_t Cb, uint8_t Cr,
                     int *ER, int *EG, int *EB)
{
    double y1, pb, pr, r, g, b;
    y1 = (255 / 219.0) * (Y - 16);
    pb = (255 / 224.0) * (Cb - 128);
    pr = (255 / 224.0) * (Cr - 128);
    r = 1.0 * y1 + 0 * pb + 1.402 * pr;
    g = 1.0 * y1 - 0.344 * pb - 0.714 * pr;
    b = 1.0 * y1 + 1.722 * pb + 0 * pr;
    *ER = clamp(r);
    *EG = clamp(g);
    *EB = clamp(b);
}

static void rgb_surface_deinit(struct sdl_ctx *c)
{
    if (!c->rgb) {
        return;
    }
    if (c->rgb->surface) {
        SDL_FreeSurface(c->rgb->surface);
    }
    if (c->rgb->pixels) {
        free(c->rgb->pixels);
    }
    free(c->rgb);
}

static int rgb_surface_init(struct sdl_ctx *c)
{
    struct sdl_rgb_ctx *rgb = CALLOC(1, struct sdl_rgb_ctx);
    rgb->rmask = 0x000000ff;
    rgb->gmask = 0x0000ff00;
    rgb->bmask = 0x00ff0000;
    rgb->amask = 0xff000000;
    rgb->width = c->width;
    rgb->height = c->height;
    rgb->bpp = c->bpp;
    rgb->pitch = c->width * 4;
    rgb->pixels_num = c->width * c->height * 4;
    rgb->pixels = (uint8_t *)calloc(1, rgb->pixels_num);
    rgb->surface = SDL_CreateRGBSurfaceFrom(rgb->pixels,
                          rgb->width,
                          rgb->height,
                          rgb->bpp,
                          rgb->pitch,
                          rgb->rmask,
                          rgb->gmask,
                          rgb->bmask,
                          rgb->amask);
    c->rgb = rgb;
    return 0;
}

static void rgb_pixels_update(struct sdl_ctx *c, void *buf)
{
    uint8_t *data = (uint8_t *)buf;
    uint8_t *pixels = c->rgb->pixels;
    int width = c->rgb->width;
    int height = c->rgb->height;
    uint8_t Y, Cr, Cb;
    int r, g, b;
    int x, y;
    int p1, p2, p3, p4;
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            p1 = y * width * 2 + x * 2;
            Y = data[p1];
            if (x % 2 == 0) {
                p2 = y * width * 2 + (x * 2 + 1);
                p3 = y * width * 2 + (x * 2 + 3);
            } else {
                p2 = y * width * 2 + (x * 2 - 1);
                p3 = y * width * 2 + (x * 2 + 1);
            }
            Cb = data[p2];
            Cr = data[p3];
            yuv2rgb(Y, Cb, Cr, &r, &g, &b);
            p4 = y * width * 4 + x * 4;
            pixels[p4] = r;
            pixels[p4 + 1] = g;
            pixels[p4 + 2] = b;
            pixels[p4 + 3] = 255;
        }
    }
}
static int rgb_surface_update(struct sdl_ctx *c, void *buf)
{
    rgb_pixels_update(c, buf);
    SDL_BlitSurface(c->rgb->surface, NULL, c->surface, NULL);
    SDL_Flip(c->surface);
    return 0;
}

static int yuv_surface_update(struct sdl_ctx *c, void *in)
{
    AVFrame *avfrm = (AVFrame *)in;
    SDL_Rect rect;
    SDL_Overlay *overlay = c->yuv->overlay;
    SDL_LockYUVOverlay(overlay);
    overlay->pixels[0] = avfrm->data[0];
    overlay->pixels[2] = avfrm->data[1];
    overlay->pixels[1] = avfrm->data[2];
    overlay->pitches[0] = avfrm->linesize[0];
    overlay->pitches[2] = avfrm->linesize[1];
    overlay->pitches[1] = avfrm->linesize[2];
    rect.x = 0;
    rect.y = 0;
    rect.w = c->width;
    rect.h = c->height;
    SDL_DisplayYUVOverlay(overlay, &rect);
    SDL_UnlockYUVOverlay(overlay);
//    SDL_Delay(40);

#if 0
    SDL_UpdateRect(c->surface,
                   rect.x, rect.y,
                   rect.w, rect.h);
#endif
    return 0;
}

static void yuv_surface_deinit(struct sdl_ctx *c)
{
    if (!c->yuv) {
        return;
    }
    if (c->yuv->overlay) {
        SDL_FreeYUVOverlay(c->yuv->overlay);
    }
    free(c->yuv);
}

static int yuv_surface_init(struct sdl_ctx *c)
{
    struct sdl_yuv_ctx *yuv = CALLOC(1, struct sdl_yuv_ctx);
    yuv->overlay = SDL_CreateYUVOverlay(c->width, c->height,
                                         SDL_YV12_OVERLAY, c->surface);
    if (yuv->overlay == NULL) {
        loge("SDL: could not create YUV overlay\n");
        return -1;
    }
    c->yuv = yuv;
    return 0;
}

static int wnd_init(struct sdl_ctx *c)
{
    int flags = SDL_SWSURFACE;// | SDL_DOUBLEBUF;
    flags |= SDL_RESIZABLE;
    if (c->type == DATA_TYPE_RGB) {
        c->bpp = 32;
    } else if (c->type == DATA_TYPE_YUV) {
        c->bpp = 24;
    }
    c->surface = SDL_SetVideoMode(c->width, c->height, c->bpp, flags);
    if (c->surface == NULL) {
        loge("SDL: could not set video mode - exiting\n");
        return -1;
    }
    if (c->type == DATA_TYPE_RGB) {
        rgb_surface_init(c);
    } else if (c->type == DATA_TYPE_YUV) {
        yuv_surface_init(c);
    }
    return 0;
}

static void sdl_deinit(struct sdl_ctx *c)
{
    if (c->event_thread) {
        SDL_WaitThread(c->event_thread, NULL);
    }

    SDL_LockMutex(c->mutex);
    if (c->type == DATA_TYPE_RGB) {
        rgb_surface_deinit(c);
    } else if (c->type == DATA_TYPE_YUV) {
        yuv_surface_deinit(c);
    }
    SDL_UnlockMutex(c->mutex);
    if (c->mutex) {
        SDL_DestroyMutex(c->mutex);
        c->mutex = NULL;
    }
    SDL_Quit();
}

static int event_thread_loop(void *arg)
{
    int quit = 0;
    struct sdl_ctx *c = (struct sdl_ctx *)arg;
    while (!quit) {
        int ret;
        SDL_Event event;
        ret = SDL_PollEvent(&event);
        if (ret < 0) {
            loge("Error when getting SDL event: %s\n", SDL_GetError());
            continue;
        }
        if (ret == 0) {
            SDL_Delay(10);
            continue;
        }

        switch (event.type) {
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
            case SDLK_q:
                quit = 1;
                break;
            default:
                break;
            }
            break;
        case SDL_QUIT:
            quit = 1;
            break;

        case SDL_VIDEORESIZE:
            //sdl->window_width  = event.resize.w;
            //sdl->window_height = event.resize.h;
            break;

        default:
            break;
        }
    }
//quit:

    SDL_LockMutex(c->mutex);
    if (c->type == DATA_TYPE_RGB) {
        rgb_surface_deinit(c);
    } else if (c->type == DATA_TYPE_YUV) {
        yuv_surface_deinit(c);
    }
    SDL_UnlockMutex(c->mutex);
    if (c->mutex) {
        SDL_DestroyMutex(c->mutex);
        c->mutex = NULL;
    }
    SDL_Quit();
    return 0;
}

static int sdl_init(struct sdl_ctx *c)
{
    int flags = SDL_INIT_VIDEO | SDL_INIT_TIMER;// | SDL_INIT_EVENTTHREAD;
    if (-1 == SDL_Init(flags)) {
        loge("Could not initialize SDL - %s\n", SDL_GetError());
        goto fail;
    }
    SDL_WM_SetCaption(wnd_title, NULL);
    if (-1 == wnd_init(c)) {
        loge("wnd_init failed\n");
        goto fail;
    }
    c->mutex = SDL_CreateMutex();
    if (!c->mutex) {
        loge("SDL_CreateMutex failed\n");
        goto fail;
    }

    c->event_thread = SDL_CreateThread(event_thread_loop, c);
    if (!c->event_thread) {
        loge("SDL_CreateThread failed\n");
        goto fail;
    }
    return 0;

fail:
    sdl_deinit(c);
    return -1;
}

static int sdl_open(struct playback_ctx *pc, const char *type, struct media_params *format)
{
    struct sdl_ctx *c = CALLOC(1, struct sdl_ctx);
    if (!c) {
        loge("malloc sdl_ctx failed!\n");
        return -1;
    }
    if (!strcmp(type, "rgb")) {
        c->type = DATA_TYPE_RGB;
        logd("use RGB surface\n");
    } else if (!strcmp(type, "yuv")) {
        c->type = DATA_TYPE_YUV;
        logd("use YUV surface\n");
    } else {
        loge("sdl only support RGB/YUV surface\n");
        return -1;
    }
    c->width = format->video.width;
    c->height = format->video.height;

    logi("sdl pix format: %d*%d\n", c->width, c->height);
    if (-1 == sdl_init(c)) {
        loge("sdl_init failed!\n");
        goto failed;
    }
    pc->priv = c;
    return 0;
failed:
    if (c) {
        free(c);
    }
    return -1;
}

static int sdl_read(struct playback_ctx *pc, void *buf, int len)
{

    return 0;
}

static int sdl_write(struct playback_ctx *pc, void *buf, int len)
{
    struct sdl_ctx *c = (struct sdl_ctx *)pc->priv;
    if (-1 == SDL_LockMutex(c->mutex)) {
        //in case mutex be destroyed in sdl event thread
        return -1;
    }
    if (c->type == DATA_TYPE_RGB) {
        rgb_surface_update(c, buf);
    } else if (c->type == DATA_TYPE_YUV) {
        yuv_surface_update(c, buf);
    }
    SDL_UnlockMutex(c->mutex);

    return 0;
}

static void sdl_close(struct playback_ctx *pc)
{
    struct sdl_ctx *c = (struct sdl_ctx *)pc->priv;
    sdl_deinit(c);
    free(c);
}

struct playback aq_sdl_playback = {
    .name = "sdl",
    .open = sdl_open,
    .read = sdl_read,
    .write = sdl_write,
    .close = sdl_close,
};
