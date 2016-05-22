/******************************************************************************
 * Copyright (C) 2014-2016
 * file:    rtsp.c
 * author:  gozfree <gozfree@163.com>
 * created: 2016-05-22 22:35
 * updated: 2016-05-22 22:35
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <pthread.h>
#include <libgzf.h>
#include <libskt.h>
#include <libgevent.h>
#include <liblog.h>
#include <libthread.h>
#include <libdict.h>
#include "protocol.h"
#include "common.h"
#include "rtsp.h"

#define RTSP_REQUEST_LEN_MAX	(1024)
#define RTSP_RESPONSE_LEN_MAX	(8192)
#define RTSP_PARAM_STRING_MAX	(200)
#define STREAM_NAME_LEN     (128)
#define DESCRIPTION_LEN     (128)


#define SDP_TOOL_NAME		((const char *)"ipcam rtsp")
#define SDP_TOOL_VERSION	((const char *)"version 2016.05.22")

#define RTSP_SERVER_IP      ((const char *)"127.0.0.1")
#define RTSP_SERVER_PORT	(8554)

typedef struct rtsp_ctx {
    int listen_fd;
    struct skt_addr host;
    struct gevent_base *evbase;
    dict *dict_client_session;
    dict *dict_media_session;
    struct protocol_ctx *rtp_ctx;
    uint16_t server_rtp_port;
    uint16_t server_rtcp_port;
} rtsp_ctx_t;

typedef enum stream_mode {
    RTP_TCP,
    RTP_UDP,
    RAW_UDP,
} stream_mode_t;

typedef struct transport_header {
    int mode;
    char *mode_str;
    char *client_dst_addr;
    uint16_t client_rtp_port;
    uint16_t client_rtcp_port;
    uint8_t  rtp_channel_id;
    uint8_t  rtcp_channel_id;
    uint8_t  client_dst_ttl;
    uint8_t  reserved;
} rtsp_transport_header_t;

typedef struct rtsp_request {
    int fd;
    struct skt_addr client;
    struct iovec *iobuf;
    char cmd[RTSP_PARAM_STRING_MAX];
    char url_prefix[RTSP_PARAM_STRING_MAX];
    char url_suffix[RTSP_PARAM_STRING_MAX];
    char cseq[RTSP_PARAM_STRING_MAX];
    char session_id[RTSP_PARAM_STRING_MAX];
    struct transport_header hdr;
    struct rtsp_ctx *rtsp_ctx;
} rtsp_request_t;

typedef struct client_session {
    uint32_t session_id;
    
} client_session_t;

typedef struct media_session {
    char stream_name[STREAM_NAME_LEN];
    char description[DESCRIPTION_LEN];
    char info[DESCRIPTION_LEN];
    struct timeval tm_create;
} media_session_t;

typedef struct media_subsession {
    char track_id[RTSP_PARAM_STRING_MAX];

} media_subsession_t;


static void url_decode(char* url)
{
    // Replace (in place) any %<hex><hex> sequences with the appropriate 8-bit character.
    char* cursor = url;
    while (*cursor) {
        if ((cursor[0] == '%') &&
             cursor[1] && isxdigit(cursor[1]) &&
             cursor[2] && isxdigit(cursor[2])) {
            // We saw a % followed by 2 hex digits, so we copy the literal hex value into the URL, then advance the cursor past it:
            char hex[3];
            hex[0] = cursor[1];
            hex[1] = cursor[2];
            hex[2] = '\0';
            *url++ = (char)strtol(hex, NULL, 16);
            cursor += 3;
        } else {
            // Common case: This is a normal character or a bogus % expression, so just copy it
            *url++ = *cursor++;
        }
    }
    *url = '\0';
}

uint32_t get_random_number()
{
  struct timeval now = {0};
  gettimeofday(&now, NULL);
  srand(now.tv_usec);
  return (rand() % ((uint32_t)-1));
}

#if 0
static void print_rtsp_request(struct rtsp_request *req)
{
#if 0
    logi("========\n");
    logi("fd = %d\n", req->fd);
    logi("client = %d:%d\n", req->client.ip, req->client.port);
    logi("cmd name = %s\n", req->cmd);
    logi("url_prefix = %s\n", req->url_prefix);
    logi("url_suffix = %s\n", req->url_suffix);
    logi("cseq = %s\n", req->cseq);
    logi("rtsp_ctx = %p\n", req->rtsp_ctx);
    logi("========\n");
#endif
}
#endif


static int parse_rtsp_request(struct rtsp_request *req)
{
    char *str = req->iobuf->iov_base;
    int len = req->iobuf->iov_len;
    uint32_t content_len = 0;
    int cmd_len = sizeof(req->cmd);
    int url_prefix_len = sizeof(req->url_prefix);
    int url_suffix_len = sizeof(req->url_suffix);
    int cseq_len = sizeof(req->cseq);
    int session_id_len = sizeof(req->session_id);
    int cur;
    char c;
    const char *rtsp_prefix = "rtsp://";
    int rtsp_prefix_len = sizeof(rtsp_prefix);

    // "Be liberal in what you accept": Skip over any whitespace at the start of the request:
    for (cur = 0; cur < len; ++cur) {
        c = str[cur];
        if (!(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\0')) {
            break;
        }
    }
    if (cur == len) {
        loge("request consisted of nothing but whitespace!\n");
        return -1;
    }

    // Then read everything up to the next space (or tab) as the command name:
    int parse_succeed = 0;
    int i;
    for (i = 0; i < cmd_len - 1 && i < len; ++cur, ++i) {
        c = str[cur];
        if (c == ' ' || c == '\t') {
            parse_succeed = 1;
            break;
        }
        req->cmd[i] = c;
    }
    req->cmd[i] = '\0';
    if (!parse_succeed) {
        loge("can't find request command name\n");
        return -1;
    }

    // Skip over the prefix of any "rtsp://" or "rtsp:/" URL that follows:
    int j = cur+1;
    while (j < len && (str[j] == ' ' || str[j] == '\t')) {
        ++j; // skip over any additional white space
    }
    for (; j < (len - rtsp_prefix_len); ++j) {
        if ((str[j] == 'r' || str[j] == 'R') &&
            (str[j+1] == 't' || str[j+1] == 'T') &&
            (str[j+2] == 's' || str[j+2] == 'S') &&
            (str[j+3] == 'p' || str[j+3] == 'P') &&
             str[j+4] == ':' && str[j+5] == '/') {
            j += 6;
            if (str[j] == '/') {
                // This is a "rtsp://" URL; skip over the host:port part that follows:
                ++j;
                while (j < len && str[j] != '/' && str[j] != ' ') ++j;
            } else {
                // This is a "rtsp:/" URL; back up to the "/":
                --j;
            }
            cur = j;
            break;
        }
    }

    // Look for the URL suffix (before the following "RTSP/"):
    parse_succeed = 0;
    int k;
    for (k = cur+1; k < (len-5); ++k) {
        if (str[k] == 'R' && str[k+1] == 'T' &&
            str[k+2] == 'S' && str[k+3] == 'P' && str[k+4] == '/') {
            while (--k >= cur && str[k] == ' ') {} // go back over all spaces before "RTSP/"
            int k1 = k;
            while (k1 > cur && str[k1] != '/') --k1;
            // ASSERT: At this point
            //   cur: first space or slash after "host" or "host:port"
            //   k: last non-space before "RTSP/"
            //   k1: last slash in the range [cur,k]
            // The URL suffix comes from [k1+1,k]
            // Copy "url_suffix":
            int n = 0, k2 = k1+1;
            if (k2 <= k) {
                if (k - k1 + 1 > url_suffix_len) return -1; // there's no room
                while (k2 <= k) req->url_suffix[n++] = str[k2++];
            }
            req->url_suffix[n] = '\0';
            // The URL 'pre-suffix' comes from [cur+1,k1-1]
            // Copy "url_prefix":
            n = 0; k2 = cur+1;
            if (k2+1 <= k1) {
                if (k1 - cur > url_prefix_len) return -1; // there's no room
                while (k2 <= k1-1) req->url_prefix[n++] = str[k2++];
            }
            req->url_prefix[n] = '\0';
            url_decode(req->url_prefix);
            cur = k + 7; // to go past " RTSP/"
            parse_succeed = 1;
            break;
        }
    }
    if (!parse_succeed) {
        return -1;
    }

    // Look for "CSeq:" (mandatory, case insensitive), skip whitespace,
    // then read everything up to the next \r or \n as 'CSeq':
    parse_succeed = 0;
    int n;
    for (j = cur; j < (len-5); ++j) {
        if (strncasecmp("CSeq:", &str[j], 5) == 0) {
            j += 5;
            while (j < len && (str[j] ==  ' ' || str[j] == '\t')) ++j;
            for (n = 0; n < cseq_len - 1 && j < len; ++n,++j) {
                c = str[j];
                if (c == '\r' || c == '\n') {
                  parse_succeed = 1;
                  break;
                }
                req->cseq[n] = c;
            }
            req->cseq[n] = '\0';
            break;
        }
    }
    if (!parse_succeed) {
        return -1;
    }

    // Look for "Session:" (optional, case insensitive), skip whitespace,
    // then read everything up to the next \r or \n as 'Session':
    req->session_id[0] = '\0'; // default value (empty string)
    for (j = cur; j < (len-8); ++j) {
        if (strncasecmp("Session:", &str[j], 8) == 0) {
            j += 8;
            while (j < len && (str[j] ==  ' ' || str[j] == '\t')) ++j;
            for (n = 0; n < session_id_len - 1 && j < len; ++n,++j) {
                c = str[j];
                if (c == '\r' || c == '\n') {
                    break;
                }
                req->session_id[n] = c;
            }
            req->session_id[n] = '\0';
            break;
        }
    }

    // Also: Look for "Content-Length:" (optional, case insensitive)
    content_len = 0; // default value
    for (j = cur; (int)j < (int)(len-15); ++j) {
        if (strncasecmp("Content-Length:", &(str[j]), 15) == 0) {
            j += 15;
            while (j < len && (str[j] ==  ' ' || str[j] == '\t')) ++j;
            int num;
            if (sscanf(&str[j], "%u", &num) == 1) {
                content_len = num;
            }
        }
    }

    logd("session_id = %s\n", req->session_id);
    logd("content_len = %d\n", content_len);
    return 0;
}

static int parse_transport(struct transport_header *hdr, char *buf, int len)
{
    uint16_t p1, p2;
    unsigned ttl, rtpCid, rtcpCid;
    hdr->mode = RTP_UDP;
    hdr->mode_str = NULL;
    hdr->client_dst_addr = NULL;
    hdr->client_dst_ttl = 255;
    hdr->client_rtp_port = 0;
    hdr->client_rtcp_port = 1;
    hdr->rtp_channel_id = 0xFF;
    hdr->rtcp_channel_id = 0xFF;

    // First, find "Transport:"
    while (1) {
        if (*buf == '\0')
            return -1; // not found
        if (*buf == '\r' && *(buf+1) == '\n' && *(buf+2) == '\r')
            return -1; // end of the headers => not found
        if (strncasecmp(buf, "Transport:", 10) == 0)
            break;
        ++buf;
    }

    // Then, run through each of the fields, looking for ones we handle:
    char const* fields = buf + 10;
    while (*fields == ' ') ++fields;
    char* field = calloc(1, strlen(fields)+1);
    while (sscanf(fields, "%[^;\r\n]", field) == 1) {
        if (strcmp(field, "RTP/AVP/TCP") == 0) {
            hdr->mode = RTP_TCP;
        } else if (strcmp(field, "RAW/RAW/UDP") == 0 ||
                   strcmp(field, "MP2T/H2221/UDP") == 0) {
            hdr->mode = RAW_UDP;
            hdr->mode_str = strdup(field);
        } else if (strncasecmp(field, "destination=", 12) == 0) {
            hdr->client_dst_addr = strdup(field+12);
        } else if (sscanf(field, "ttl%u", &ttl) == 1) {
            hdr->client_dst_ttl = (uint8_t)ttl;
        } else if (sscanf(field, "client_port=%hu-%hu", &p1, &p2) == 2) {
            hdr->client_rtp_port = p1;
            hdr->client_rtcp_port = hdr->mode == RAW_UDP ? 0 : p2; // ignore the second port number if the client asked for raw UDP
        } else if (sscanf(field, "client_port=%hu", &p1) == 1) {
            hdr->client_rtp_port = p1;
            hdr->client_rtcp_port = hdr->mode == RAW_UDP ? 0 : p1 + 1;
        } else if (sscanf(field, "interleaved=%u-%u", &rtpCid, &rtcpCid) == 2) {
            hdr->rtp_channel_id = (unsigned char)rtpCid;
            hdr->rtcp_channel_id = (unsigned char)rtcpCid;
        }

        fields += strlen(field);
        while (*fields == ';' || *fields == ' ' || *fields == '\t') ++fields; // skip over separating ';' chars or whitespace
        if (*fields == '\0' || *fields == '\r' || *fields == '\n') break;
    }
    free(field);
    logd("parse_transport finished\n");
    logd("mode = %s\n", hdr->mode_str);
    logd("rtp_port = %d\n", hdr->client_rtp_port);
    logd("rtcp_port = %d\n", hdr->client_rtcp_port);
    logd("rtp_channel_id = %d\n", hdr->rtp_channel_id);
    logd("rtcp_channel_id = %d\n", hdr->rtcp_channel_id);
    return 0;
}

static char const* get_date_string()
{
    static char buf[128];
    time_t cur_time = time(NULL);
    strftime(buf, sizeof(buf), "Date: %a, %b %d %Y %H:%M:%S GMT", gmtime(&cur_time));
    return buf;
}

static struct media_session *media_session_create(struct rtsp_ctx *rc, char *stream_name, size_t size)
{
    struct media_session *ms = CALLOC(1, struct media_session);
    snprintf(ms->stream_name, size, "%s", stream_name);
    snprintf(ms->description, sizeof(ms->description), "%s", "Session streamd by ipcam");
    snprintf(ms->info, sizeof(ms->info), "%s", stream_name);
    gettimeofday(&ms->tm_create, NULL);
    dict_add(rc->dict_media_session, stream_name, (char *)ms);
    return ms;
}

static void media_session_destroy(struct rtsp_ctx *rc, char *stream_name)
{
    //TODO
}

static char *strcat_url(char *dest, char *prefix, char *suffix)
{
    logd("prefix=%s, suffix=%s\n", prefix, suffix);
    dest[0] = '\0';
    if (prefix[0] != '\0') {
        strcat(dest, prefix);
        strcat(dest, "/");
    }
    return strcat(dest, suffix);
}

static struct media_session *media_session_lookup(struct rtsp_ctx *rc, struct rtsp_request *req)
{
    char url_total[2*RTSP_PARAM_STRING_MAX];
    // enough space for url_prefix/url_suffix'\0'
    strcat_url(url_total, req->url_prefix, req->url_suffix);
    logd("url_total = %s\n", url_total);
    struct media_session *ms = (struct media_session *)dict_get(rc->dict_media_session, url_total, NULL);
    return ms;
}



static int handle_not_found(struct rtsp_request *req, char *buf, size_t size)
{
    snprintf(buf, size, RESP_NOT_FOUND_FMT,
             req->cseq, get_date_string());
    return 0;
}

static int handle_session_not_found(struct rtsp_request *req, char *buf, size_t size)
{
    snprintf(buf, size, RESP_SESSION_NOT_FOUND_FMT,
             req->cseq, get_date_string());
    return 0;
}

#if 0
static int handle_bad_request(struct rtsp_request *req, char *buf, size_t size)
{
    snprintf(buf, size, RESP_BAD_REQUEST_FMT,
             get_date_string(), ALLOWED_COMMAND);
    return 0;
}
static int handle_not_supported(struct rtsp_request *req, char *buf, size_t size)
{
    snprintf(buf, size, RESP_NOT_SUPPORTED_FMT,
             req->cseq, get_date_string(), ALLOWED_COMMAND);
    return 0;
}


static int handle_unsupported_transport(struct rtsp_request *req, char *buf, size_t size)
{
    snprintf(buf, size, RESP_UNSUPPORTED_TRANSPORT_FMT,
             req->cseq, get_date_string());
    return 0;
}
#endif

static int handle_options(struct rtsp_request *req, char *buf, size_t size)
{
    snprintf(buf, size, RESP_OPTIONS_FMT,
             req->cseq, get_date_string(), ALLOWED_COMMAND);
    return 0;
}

static char *get_rtsp_url(struct rtsp_request *req, struct media_session *ms, char *buf, size_t size)
{
    const char *host = RTSP_SERVER_IP;
    int port = req->rtsp_ctx->host.port;
    char url_total[2*RTSP_PARAM_STRING_MAX];
    memset(url_total, 0, sizeof(url_total));
    if (port == 554) {
        snprintf(buf, size, "rtsp://%s/%s", host, strcat_url(url_total, req->url_prefix, ms->stream_name));
    } else {
        snprintf(buf, size, "rtsp://%s:%hu/%s", host, port, strcat_url(url_total, req->url_prefix, ms->stream_name));
    }
    logd("RTSP URL: %s\n", buf);
    return buf;
}

static float duration()
{
    return 0.0;
}

static char *get_sdp(struct media_session *ms)
{
    char *sdp = NULL;
    int sdp_len = 0;
    char sdp_filter[128];
    char sdp_range[128];
    char *sdp_media = "m=video 0 RTP/AVP 33\r\nc=IN IP4 0.0.0.0\r\nb=AS:5000\r\na=control:track1";

    float dur = duration();
    if (dur == 0.0) {
      snprintf(sdp_range, sizeof(sdp_range), "%s", "a=range:npt=0-\r\n");
    } else if (dur > 0.0) {
      snprintf(sdp_range, sizeof(sdp_range), "%s", "a=range:npt=0-%.3f\r\n");
    } else { // subsessions have differing durations, so "a=range:" lines go there
      snprintf(sdp_range, sizeof(sdp_range), "%s", "");
    }
    snprintf(sdp_filter, sizeof(sdp_filter), SDP_FILTER_FMT, RTSP_SERVER_IP);

    char const* const sdp_prefix_fmt =
      "v=0\r\n"
      "o=- %ld%06ld %d IN IP4 %s\r\n"
      "s=%s\r\n"
      "i=%s\r\n"
      "t=0 0\r\n"
      "a=tool:%s %s\r\n"
      "a=type:broadcast\r\n"
      "a=control:*\r\n"
      "%s"
      "%s"
      "a=x-qt-text-nam:%s\r\n"
      "a=x-qt-text-inf:%s\r\n"
      "%s";

    sdp_len = strlen(sdp_prefix_fmt)
      + 20 + 6 + 20 + strlen("127.0.0.1")
      + strlen(ms->description)
      + strlen(ms->info)
      + strlen(SDP_TOOL_NAME) + strlen(SDP_TOOL_VERSION)
      + strlen(sdp_filter)
      + strlen(sdp_range)
      + strlen(ms->description)
      + strlen(ms->info)
      + strlen(sdp_media);
    logd("sdp_len = %d\n", sdp_len);
    sdp = calloc(1, sdp_len);

    // Generate the SDP prefix (session-level lines):
    snprintf(sdp, sdp_len, sdp_prefix_fmt,
        ms->tm_create.tv_sec, ms->tm_create.tv_usec,// o= <session id>
	     1, // o= <version>
	     RTSP_SERVER_IP, // o= <address>
	     ms->description, // s= <description>
	     ms->info, // i= <info>
	     SDP_TOOL_NAME, SDP_TOOL_VERSION, // a=tool:
	     sdp_filter, // a=source-filter: incl (if a SSM session)
	     sdp_range, // a=range: line
	     ms->description, // a=x-qt-text-nam: line
	     ms->info, // a=x-qt-text-inf: line
	     sdp_media); // miscellaneous session SDP lines (if any)
    logd("strlen sdp = %d\n", strlen(sdp));

  return sdp;
}

static int handle_describe(struct rtsp_request *req, char *buf, uint32_t size)
{
    struct rtsp_ctx *rc = req->rtsp_ctx;
    char rtspurl[1024]   = {0};
    struct media_session *ms = media_session_lookup(rc, req);
    if (!ms) {
        logi("media_session_lookup find nothting\n");
        handle_not_found(req, buf, size);
        return -1;
    }
    logi("media_session_lookup found\n");
    char *sdp = get_sdp(ms);
    //XXX: sdp line can't using "\r\n" as ending!!!
    snprintf(buf, size, RESP_DESCRIBE_FMT,
                 req->cseq,
                 get_date_string(),
                 get_rtsp_url(req, ms, rtspurl, sizeof(rtspurl)),
                 (uint32_t)strlen(sdp),
                 sdp);
    return 0;
}

static struct client_session *client_session_create(struct rtsp_request *req)
{
    char session_id[9];
    struct rtsp_ctx *rc = req->rtsp_ctx;
    struct client_session *cs = CALLOC(1, struct client_session);
    struct client_session *tmp;
    do {
        cs->session_id = get_random_number();
        snprintf(session_id, sizeof(session_id), "%08x", cs->session_id);
        tmp = (struct client_session *)dict_get(rc->dict_client_session, session_id, NULL);
    } while (tmp != NULL);
    dict_add(rc->dict_client_session, session_id, (char *)cs);

    return cs;
}

static void client_session_destroy(struct client_session *cs)
{

}

static struct client_session *client_session_lookup(struct rtsp_ctx *rc, struct rtsp_request *req)
{
    struct client_session *cs = (struct client_session *)dict_get(rc->dict_client_session, req->session_id, NULL);
    return cs;
}

static int handle_setup(struct rtsp_request *req, char *buf, uint32_t size)
{
    char *mode_string = NULL;
    const char *dst_ip = RTSP_SERVER_IP;
    const char *client_ip = RTSP_SERVER_IP;
    struct rtsp_ctx *rc = req->rtsp_ctx;
    rc->server_rtp_port = 6970;
    rc->server_rtcp_port = rc->server_rtp_port + 1;
    int port_rtp = rc->server_rtp_port;
    int port_rtcp =  rc->server_rtcp_port;

    parse_transport(&req->hdr, req->iobuf->iov_base, req->iobuf->iov_len);
    struct client_session *cs = client_session_lookup(rc, req);
    if (!cs) {
        loge("client_session_lookup find nothting\n");
        cs = client_session_create(req);
        if (!cs) {
            loge("client_session_create failed\n");
            handle_session_not_found(req, buf, size);
            return -1;
        }
    }
    struct media_session *ms = media_session_lookup(rc, req);
    if (!ms) {
        loge("media_session_lookup find nothting\n");
        handle_not_found(req, buf, size);
        return -1;
    }
    //TODO: handle subsession
    if (req->hdr.client_dst_addr != NULL) {
        dst_ip = req->hdr.client_dst_addr;
    }
    switch (req->hdr.mode) {
    case RTP_TCP:
        mode_string = "RTP/AVP/TCP";
        break;
    case RTP_UDP:
        mode_string = "RTP/AVP";
        break;
    case RAW_UDP:
        mode_string = req->hdr.mode_str;
        break;
    default:
        break;
    }
    skt_udp_bind(NULL, port_rtp, 1);
    skt_udp_bind(NULL, port_rtcp, 1);

    snprintf(buf, size, RESP_SETUP_FMT,
                 req->cseq,
                 get_date_string(),
                 mode_string,
                 dst_ip,
                 client_ip,
                 req->hdr.client_rtp_port,
                 req->hdr.client_rtcp_port,
                 port_rtp, port_rtcp,
                 cs->session_id);
    return 0;
}

static int handle_teardown(struct rtsp_request *req, char *buf, uint32_t size)
{
    client_session_destroy(NULL);
    return 0;
}

static uint32_t conv_to_rtp_timestamp(struct timeval tv)
{
    uint32_t ts_freq = 1;//audio sample rate
    uint32_t ts_inc = (ts_freq*tv.tv_sec);
    ts_inc += (uint32_t)(ts_freq*(tv.tv_usec/1000000.0) + 0.5);
    return ts_inc;
}

static uint32_t get_timestamp()
{
    struct timeval now;
    gettimeofday(&now, NULL);

    return conv_to_rtp_timestamp(now);
}

static char *get_rtp_info(struct rtsp_request *req, char *buf, size_t size)
{
    char rtspurl[1024] = {0};
    char const* rtp_info_fmt =
    "RTP-Info: " // "RTP-Info:", plus any preceding rtpInfo items
    "url=%s/%s"
    ";seq=%d"
    ";rtptime=%u";
    struct rtsp_ctx *rc = req->rtsp_ctx;
    struct media_session *ms = media_session_lookup(rc, req);
    if (!ms) {
        loge("media_session_lookup find nothting\n");
        return NULL;
    }
    snprintf(buf, size, rtp_info_fmt,
             get_rtsp_url(req, ms, rtspurl, sizeof(rtspurl)),
             "track1",
             req->cseq,
             get_timestamp());
    return buf;
}

static int handle_play(struct rtsp_request *req, char *buf, uint32_t size)
{
    char rtp_info[1024] = {0};

    snprintf(buf, size, RESP_PLAY_FMT,
               req->cseq,
               get_date_string(),
               (uint32_t)strtol(req->session_id, NULL, 16),
               get_rtp_info(req, rtp_info, sizeof(rtp_info)));
    char rtp_url[128] = {0};
    memset(rtp_url, 0, sizeof(rtp_url));

#if 0
    struct rtsp_ctx *rc = req->rtsp_ctx;
    snprintf(rtp_url, sizeof(rtp_url), "rtp://127.0.0.1:%d", rc->server_rtp_port);
    struct protocol_ctx *pc = protocol_new(rtp_url);
    logi("protocol_new success!\n");
    if (-1 == protocol_open(pc)) {
        logi("protocol_open rtp failed!\n");
        return -1;
    }
    logi("protocol_open rtp success!\n");
    req->rtsp_ctx->rtp_ctx = pc;
    logi("rtp_ctx = %p\n", req->rtsp_ctx->rtp_ctx);
#endif

    return 0;
}

static int handle_get_parameter(struct rtsp_request *req, char *buf, uint32_t size)
{
    return 0;
}
static int handle_rtsp_request(struct rtsp_request *req)
{
    char resp[RTSP_RESPONSE_LEN_MAX];
    memset(resp, 0, sizeof(resp));

    if (strcmp(req->cmd, "OPTIONS") == 0) {
        handle_options(req, resp, sizeof(resp));
    } else if (strcmp(req->cmd, "DESCRIBE") == 0) {
        handle_describe(req, resp, sizeof(resp));
    } else if (strcmp(req->cmd, "SETUP") == 0) {
        handle_setup(req, resp, sizeof(resp));
    } else if (strcmp(req->cmd, "TEARDOWN") == 0) {
        handle_teardown(req, resp, sizeof(resp));
    } else if (strcmp(req->cmd, "PLAY") == 0) {
        handle_play(req, resp, sizeof(resp));
    } else if (strcmp(req->cmd, "GET_PARAMETER") == 0) {
        handle_get_parameter(req, resp, sizeof(resp));
    }
    logi("rtsp response len = %d buf:\n%s\n", strlen(resp), resp);
    int ret = skt_send(req->fd, resp, strlen(resp));
    logd("skt_send fd = %d, len = %d\n", req->fd, ret);
    return 0;
}

static void on_recv(int fd, void *arg)
{
    int rlen, res;
    struct rtsp_request *req = (struct rtsp_request *)arg;
    memset(req->iobuf->iov_base, 0, RTSP_REQUEST_LEN_MAX);
    rlen = skt_recv(fd, req->iobuf->iov_base, RTSP_REQUEST_LEN_MAX);
    if (rlen > 0) {
        logi("rtsp request message len = %d, buf:\n%s\n", rlen, (char *)req->iobuf->iov_base);
        req->iobuf->iov_len = rlen;
        res = parse_rtsp_request(req);
        if (res == -1) {
            loge("parse_rtsp_request failed\n");
            return;
        }
        res = handle_rtsp_request(req);
        if (res == -1) {
            loge("handle_rtsp_request failed\n");
            return;
        }
    } else if (rlen == 0) {
        loge("peer connect shutdown\n");
    } else {
        loge("something error\n");
    }
}

static void on_error(int fd, void *arg)
{
    loge("error: %d\n", errno);
}

static struct iovec *iovec_create(size_t len)
{
    struct iovec *vec = CALLOC(1, struct iovec);
    vec->iov_len = len;
    vec->iov_base = calloc(1, len);
    return vec;
}

static void rtsp_connect_create(struct rtsp_ctx *rtsp, int fd, uint32_t ip, uint16_t port)
{
    struct rtsp_request *req = CALLOC(1, struct rtsp_request);
    req->fd = fd;
    req->client.ip = ip;
    req->client.port = port;
    req->rtsp_ctx = rtsp;
    req->iobuf = iovec_create(RTSP_REQUEST_LEN_MAX);
    skt_set_noblk(fd, 1);
    struct gevent *e = gevent_create(fd, on_recv, NULL, on_error, req);
    if (-1 == gevent_add(rtsp->evbase, e)) {
        loge("event_add failed!\n");
    }
}

static void on_connect(int fd, void *arg)
{
    int afd;
    uint32_t ip;
    uint16_t port;
    struct rtsp_ctx *rtsp = (struct rtsp_ctx *)arg;

    afd = skt_accept(fd, &ip, &port);
    if (afd == -1) {
        loge("skt_accept failed: %d\n", errno);
        return;
    }
    rtsp_connect_create(rtsp, afd, ip, port);
}

static void *rtsp_thread_event(struct thread *t, void *arg)
{
    struct rtsp_ctx *rc = (struct rtsp_ctx *)arg;
    gevent_base_loop(rc->evbase);
    return NULL;
}

static int rtsp_open(struct protocol_ctx *pc, const char *url, struct media_params *media)
{
    int fd;
    const char *ip = RTSP_SERVER_IP;
    int port = RTSP_SERVER_PORT;
    char *stream_name = "test.ts";
    struct rtsp_ctx *rc = CALLOC(1, struct rtsp_ctx);
    if (!rc) {
        loge("malloc rtsp_ctx failed!\n");
        return -1;
    }
    fd = skt_tcp_bind_listen(NULL, port, 1);
    if (fd == -1) {
        goto failed;
    }
    logi("rtsp_ctx addr = %p\n", rc);
    rc->host.port = port;
    rc->dict_client_session = dict_new();
    rc->dict_media_session = dict_new();
    media_session_create(rc, stream_name, sizeof(stream_name));
    logi("rtsp url = %s listen port = %d\n", url, port);
    rc->listen_fd = fd;
    rc->evbase = gevent_base_create();
    if (!rc->evbase) {
        goto failed;
    }
    struct gevent *e = gevent_create(fd, on_connect, NULL, on_error, (void *)rc);
    if (-1 == gevent_add(rc->evbase, e)) {
        loge("event_add failed!\n");
        gevent_destroy(e);
    }
    logi("rtsp://%s:%d/test.ts\n", ip, port);
    thread_create("rtsp_event_loop", rtsp_thread_event, rc);
    pc->priv = rc;
    return 0;
failed:
    if (fd != -1) {
        skt_close(fd);
    }
    if (rc) {
        free(rc);
    }
    return -1;
}

static int rtsp_read(struct protocol_ctx *pc, void *buf, int len)
{
    return 0;
}

static int rtsp_write(struct protocol_ctx *pc, void *buf, int len)
{
#if 0
    struct rtsp_ctx *rc = pc->priv;
    logi("rc = %p\n", rc);
    struct protocol_ctx *rtp_ctx = rc->rtp_ctx;
    logi("rtp_ctx = %p\n", rtp_ctx);
    if (!rtp_ctx) {
        return -1;
    }
    protocol_write(rtp_ctx, buf, len);
#endif
    return 0;
}

static void rtsp_close(struct protocol_ctx *pc)
{
    struct rtsp_ctx *rc = (struct rtsp_ctx *)pc->priv;
    media_session_destroy(rc, NULL);
    free(rc);
}

struct protocol aq_rtsp_protocol = {
    .name = "rtsp",
    .open = rtsp_open,
    .read = rtsp_read,
    .write = rtsp_write,
    .close = rtsp_close,
};
