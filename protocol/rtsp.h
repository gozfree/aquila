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
#ifndef _RTSP_H_
#define _RTSP_H_


#ifdef __cplusplus
extern "C" {
#endif


#define  ALLOWED_COMMAND \
    "OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, GET_PARAMETER, SET_PARAMETER"

#define RESP_BAD_REQUEST_FMT    ((const char *) \
        "RTSP/1.0 400 Bad Request\r\n"          \
        "%s\r\n"                                \
        "Allow: %s\r\n\r\n")

#define RESP_NOT_SUPPORTED_FMT  ((const char *) \
        "RTSP/1.0 405 Method Not Allowed\r\n"   \
        "CSeq: %s\r\n"                          \
        "%s\r\n"                                \
        "Allow: %s\r\n\r\n")

#define RESP_NOT_FOUND_FMT      ((const char *) \
        "RTSP/1.0 404 Stream Not Found\r\n"     \
        "CSeq: %s\r\n"                          \
        "%s\r\n")

#define RESP_SESSION_NOT_FOUND_FMT ((const char *) \
        "RTSP/1.0 454 Session Not Found\r\n"     \
        "CSeq: %s\r\n"                          \
        "%s\r\n")

#define RESP_UNSUPPORTED_TRANSPORT_FMT ((const char *) \
        "RTSP/1.0 461 Unsupported Transport\r\n"\
        "CSeq: %s\r\n"                          \
        "%s\r\n")

#define RESP_OPTIONS_FMT    ((const char *)     \
        "RTSP/1.0 200 OK\r\n"                   \
        "CSeq: %s\r\n"                          \
        "%s\r\n"                                \
        "Public: %s\r\n\r\n")

#define RESP_DESCRIBE_FMT   ((const char *)     \
        "RTSP/1.0 200 OK\r\n"                   \
        "CSeq: %s\r\n"                          \
        "%s\r\n"                                \
        "Content-Base: %s\r\n"                  \
        "Content-Type: application/sdp\r\n"     \
        "Content-Length: %u\r\n\r\n"            \
        "%s")

#define RESP_SETUP_FMT      ((const char *)     \
        "RTSP/1.0 200 OK\r\n"                   \
        "CSeq: %s\r\n"                          \
        "%s\r\n"                                \
        "Transport: %s;unicast;destination=%s;source=%s;"   \
        "client_port=%hu-%hu;server_port=%hu-%hu\r\n"   \
        "Session: %08X\r\n\r\n")

#define RESP_PLAY_FMT       ((const char *)     \
        "RTSP/1.0 200 OK\r\n"                   \
        "CSeq: %s\r\n"                          \
        "%s\r\n"                                \
        "Range: npt=0.000-\r\n"                 \
        "Session: %08X\r\n"                     \
        "%s\r\n\r\n")

#define SDP_FILTER_FMT      ((const char *)     \
        "a=source-filter: incl IN IP4 * %s\r\n" \
        "a=rtcp-unicast: reflection\r\n")



#ifdef __cplusplus
}
#endif
#endif
