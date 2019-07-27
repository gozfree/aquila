###############################################################################
# common
###############################################################################
#ARCH: linux/pi/android/ios/
ARCH		?= linux
CROSS_PREFIX	?=
OUTPUT		?= /usr/local
BUILD_DIR	:= $(shell pwd)/../build/
ARCH_INC	:= $(BUILD_DIR)/$(ARCH).inc
COLOR_INC	:= $(BUILD_DIR)/color.inc

ifeq ($(ARCH_INC), $(wildcard $(ARCH_INC)))
include $(ARCH_INC)
endif

CC	= $(CROSS_PREFIX)gcc
CXX	= $(CROSS_PREFIX)g++
LD	= $(CROSS_PREFIX)ld
AR	= $(CROSS_PREFIX)ar

ifeq ($(COLOR_INC), $(wildcard $(COLOR_INC)))
include $(COLOR_INC)
else
CC_V	= $(CC)
CXX_V	= $(CXX)
LD_V	= $(LD)
AR_V	= $(AR)
CP_V	= $(CP)
RM_V	= $(RM)
endif

###############################################################################
# target and object
###############################################################################
TGT_NAME 	= aquila
###############################################################################
# cflags and ldflags
###############################################################################
CFLAGS_SDL	= `pkg-config --cflags sdl`
LDFLAGS_SDL	= `pkg-config --libs sdl`
LDFLAGS_X264	= -lx264
LDFLAGS_ALSA	= -lasound
LDFLAGS_FFMPEG	= `pkg-config --libs libavformat libavutil libavcodec libswscale`
LDFLAGS_JPEG	= -ljpeg
LDFLAGS_LUA 	= -llua5.2

CFLAGS	:= -g -Wall
# -Werror
CFLAGS	+= -I./
CFLAGS	+= -I./algo
CFLAGS	+= -I./codec
CFLAGS	+= -I./device
CFLAGS	+= -I./filter
CFLAGS	+= -I./muxer
CFLAGS	+= -I./playback
CFLAGS	+= -I./protocol
CFLAGS	+= -I./util

CFLAGS	+= -Wl,-rpath=/usr/loca/lib
CFLAGS	+= $(CFLAGS_SDL)

LDFLAGS	:=
LDFLAGS	+= -lgcc_s -lc
LDFLAGS += -L$(OUTPUT)/lib
LDFLAGS	+= -ldebug -llog -lconfig -lgevent -ltime -ldict -lvector -lskt
LDFLAGS	+= -lthread -lmacro -lrpc -lhash -lworkq -luvc
LDFLAGS	+= -L/usr/local/lib/ -lrtmp
LDFLAGS	+= -lqueue
LDFLAGS	+= -lpthread -lrt
LDFLAGS	+= $(LDFLAGS_SDL)
LDFLAGS	+= $(LDFLAGS_X264)
LDFLAGS += $(LDFLAGS_ALSA)
LDFLAGS += $(LDFLAGS_FFMPEG)
LDFLAGS += $(LDFLAGS_JPEG)
LDFLAGS	+= $(LDFLAGS_LUA)

.PHONY : all clean

TGT	:= $(TGT_NAME)

ALGO_OBJS :=

CODEC_OBJS := 			    \
    codec/codec.o 		    \
    codec/x264_enc.o		\
    codec/h264_dec.o		\
    codec/h264_enc.o		\
    codec/mjpeg_enc.o

DEVICE_OBJS := 			\
    device/device.o 		\
    device/usbcam.o 		\
    device/vdevfake.o

FILTER_OBJS := 			\
    filter/filter.o 		\
    filter/videocap_filter.o 	\
    filter/vencode_filter.o 	\
    filter/vdecode_filter.o 	\
    filter/upstream_filter.o 	\
    filter/record_filter.o 	\
    filter/playback_filter.o 	\
    filter/remotectrl_filter.o

MUXER_OBJS :=			\
    muxer/muxer.o		\
    muxer/mp4.o			\
    muxer/flv.o


PLAYBACK_OBJS := 		\
    playback/playback.o 	\
    playback/sdl.o		\
    playback/snkfake.o

PROTOCOL_OBJS :=		\
    protocol/protocol.o		\
    protocol/rtmp.o		\
    protocol/rtsp.o		\
    protocol/rtp.o		\
    protocol/rtp_h264.o		\
    protocol/rpcd.o

UTIL_OBJS := 			\
    util/url.o 			\
    util/config.o		\
    util/imgconvert.o		\
    util/overlay.o

OBJS := \
    $(ALGO_OBJS) 		\
    $(CODEC_OBJS) 		\
    $(DEVICE_OBJS) 		\
    $(FILTER_OBJS) 		\
    $(MUXER_OBJS) 		\
    $(PLAYBACK_OBJS) 		\
    $(PROTOCOL_OBJS) 		\
    $(UTIL_OBJS) 		\
    main.o

all: $(TGT)

%.o:%.c
	$(CC_V) -c $(CFLAGS) $< -o $@

%.o:%.cc
	$(CXX_V) -c $(CFLAGS) $< -o $@

%.o:%.cpp
	$(CXX_V) -c $(CFLAGS) $< -o $@


$(TGT_NAME): $(OBJS)
	$(CC_V) -o $@ $^ $(LDFLAGS)

clean:
	$(RM_V) -f $(OBJS)
	$(RM_V) -f $(TGT)

install:
	$(CP_V) -f $(TGT_NAME) ${OUTPUT}/bin

uninstall:
	$(RM_V) -f ${OUTPUT}/bin/$(TGT_NAME)

