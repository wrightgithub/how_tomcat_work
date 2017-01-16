/*
 *  Licensed to the Apache Software Foundation (ASF) under one or more
 *  contributor license agreements.  See the NOTICE file distributed with
 *  this work for additional information regarding copyright ownership.
 *  The ASF licenses this file to You under the Apache License, Version 2.0
 *  (the "License"); you may not use this file except in compliance with
 *  the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

/***************************************************************************
 * Description: common stuff for bi-directional protocol ajp13/ajp14.      *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision: 749685 $                                          *
 ***************************************************************************/

#ifndef JK_AJP_COMMON_H
#define JK_AJP_COMMON_H

#include "jk_service.h"
#include "jk_msg_buff.h"
#include "jk_shm.h"
#include "jk_mt.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define JK_AJP_STATE_IDLE               (0)
#define JK_AJP_STATE_OK                 (1)
#define JK_AJP_STATE_ERROR              (2)
#define JK_AJP_STATE_PROBE              (3)
#define JK_AJP_STATE_DEF                (JK_AJP_STATE_IDLE)
#define JK_AJP_STATE_TEXT_IDLE          ("OK/IDLE")
#define JK_AJP_STATE_TEXT_OK            ("OK")
#define JK_AJP_STATE_TEXT_ERROR         ("ERR")
#define JK_AJP_STATE_TEXT_PROBE         ("ERR/PRB")
#define JK_AJP_STATE_TEXT_MAX           (JK_AJP_STATE_PROBE)
#define JK_AJP_STATE_TEXT_DEF           (JK_AJP_STATE_TEXT_IDLE)

/* We accept doing global maintenance if we are */
/* JK_AJP_MAINTAIN_TOLERANCE seconds early. */
#define JK_AJP_MAINTAIN_TOLERANCE (2)

/*
 * Conditional request attributes
 *
 */
#define SC_A_CONTEXT            (unsigned char)1
#define SC_A_SERVLET_PATH       (unsigned char)2
#define SC_A_REMOTE_USER        (unsigned char)3
#define SC_A_AUTH_TYPE          (unsigned char)4
#define SC_A_QUERY_STRING       (unsigned char)5
#define SC_A_ROUTE              (unsigned char)6
#define SC_A_SSL_CERT           (unsigned char)7
#define SC_A_SSL_CIPHER         (unsigned char)8
#define SC_A_SSL_SESSION        (unsigned char)9
#define SC_A_REQ_ATTRIBUTE      (unsigned char)10
#define SC_A_SSL_KEY_SIZE       (unsigned char)11       /* only in if JkOptions +ForwardKeySize */
#define SC_A_SECRET             (unsigned char)12
#define SC_A_STORED_METHOD      (unsigned char)13
#define SC_A_ARE_DONE           (unsigned char)0xFF

/*
 * Request methods, coded as numbers instead of strings.
 * The list of methods was taken from Section 5.1.1 of RFC 2616,
 * RFC 2518, the ACL IETF draft, and the DeltaV IESG Proposed Standard.
 *          Method        = "OPTIONS"
 *                        | "GET"
 *                        | "HEAD"
 *                        | "POST"
 *                        | "PUT"
 *                        | "DELETE"
 *                        | "TRACE"
 *                        | "PROPFIND"
 *                        | "PROPPATCH"
 *                        | "MKCOL"
 *                        | "COPY"
 *                        | "MOVE"
 *                        | "LOCK"
 *                        | "UNLOCK"
 *                        | "ACL"
 *                        | "REPORT"
 *                        | "VERSION-CONTROL"
 *                        | "CHECKIN"
 *                        | "CHECKOUT"
 *                        | "UNCHECKOUT"
 *                        | "SEARCH"
 *                        | "MKWORKSPACE"
 *                        | "UPDATE"
 *                        | "LABEL"
 *                        | "MERGE"
 *                        | "BASELINE-CONTROL"
 *                        | "MKACTIVITY"
 *
 */
#define SC_M_OPTIONS            (unsigned char)1
#define SC_M_GET                (unsigned char)2
#define SC_M_HEAD               (unsigned char)3
#define SC_M_POST               (unsigned char)4
#define SC_M_PUT                (unsigned char)5
#define SC_M_DELETE             (unsigned char)6
#define SC_M_TRACE              (unsigned char)7
#define SC_M_PROPFIND           (unsigned char)8
#define SC_M_PROPPATCH          (unsigned char)9
#define SC_M_MKCOL              (unsigned char)10
#define SC_M_COPY               (unsigned char)11
#define SC_M_MOVE               (unsigned char)12
#define SC_M_LOCK               (unsigned char)13
#define SC_M_UNLOCK             (unsigned char)14
#define SC_M_ACL                (unsigned char)15
#define SC_M_REPORT             (unsigned char)16
#define SC_M_VERSION_CONTROL    (unsigned char)17
#define SC_M_CHECKIN            (unsigned char)18
#define SC_M_CHECKOUT           (unsigned char)19
#define SC_M_UNCHECKOUT         (unsigned char)20
#define SC_M_SEARCH             (unsigned char)21
#define SC_M_MKWORKSPACE        (unsigned char)22
#define SC_M_UPDATE             (unsigned char)23
#define SC_M_LABEL              (unsigned char)24
#define SC_M_MERGE              (unsigned char)25
#define SC_M_BASELINE_CONTROL   (unsigned char)26
#define SC_M_MKACTIVITY         (unsigned char)27
#define SC_M_JK_STORED          (unsigned char)0xFF

/*
 * Frequent request headers, these headers are coded as numbers
 * instead of strings.
 *
 * Accept
 * Accept-Charset
 * Accept-Encoding
 * Accept-Language
 * Authorization
 * Connection
 * Content-Type
 * Content-Length
 * Cookie
 * Cookie2
 * Host
 * Pragma
 * Referer
 * User-Agent
 *
 */

#define SC_ACCEPT               (unsigned short)0xA001
#define SC_ACCEPT_CHARSET       (unsigned short)0xA002
#define SC_ACCEPT_ENCODING      (unsigned short)0xA003
#define SC_ACCEPT_LANGUAGE      (unsigned short)0xA004
#define SC_AUTHORIZATION        (unsigned short)0xA005
#define SC_CONNECTION           (unsigned short)0xA006
#define SC_CONTENT_TYPE         (unsigned short)0xA007
#define SC_CONTENT_LENGTH       (unsigned short)0xA008
#define SC_COOKIE               (unsigned short)0xA009
#define SC_COOKIE2              (unsigned short)0xA00A
#define SC_HOST                 (unsigned short)0xA00B
#define SC_PRAGMA               (unsigned short)0xA00C
#define SC_REFERER              (unsigned short)0xA00D
#define SC_USER_AGENT           (unsigned short)0xA00E

/*
 * Frequent response headers, these headers are coded as numbers
 * instead of strings.
 *
 * Content-Type
 * Content-Language
 * Content-Length
 * Date
 * Last-Modified
 * Location
 * Set-Cookie
 * Servlet-Engine
 * Status
 * WWW-Authenticate
 *
 */

#define SC_RESP_CONTENT_TYPE        (unsigned short)0xA001
#define SC_RESP_CONTENT_LANGUAGE    (unsigned short)0xA002
#define SC_RESP_CONTENT_LENGTH      (unsigned short)0xA003
#define SC_RESP_DATE                (unsigned short)0xA004
#define SC_RESP_LAST_MODIFIED       (unsigned short)0xA005
#define SC_RESP_LOCATION            (unsigned short)0xA006
#define SC_RESP_SET_COOKIE          (unsigned short)0xA007
#define SC_RESP_SET_COOKIE2         (unsigned short)0xA008
#define SC_RESP_SERVLET_ENGINE      (unsigned short)0xA009
#define SC_RESP_STATUS              (unsigned short)0xA00A
#define SC_RESP_WWW_AUTHENTICATE    (unsigned short)0xA00B
#define SC_RES_HEADERS_NUM          11

/*
 * AJP13/AJP14 use same message structure
 */

#define AJP_DEF_RETRY_ATTEMPTS    (1)

#define AJP_HEADER_LEN            (4)
#define AJP_HEADER_SZ_LEN         (2)
#define CHUNK_BUFFER_PAD          (12)
#define AJP_DEF_CACHE_TIMEOUT     (0)
#define AJP_DEF_CONNECT_TIMEOUT   (0)   /* NO CONNECTION TIMEOUT => NO CPING/CPONG */
#define AJP_DEF_REPLY_TIMEOUT     (0)   /* NO REPLY TIMEOUT                        */
#define AJP_DEF_PREPOST_TIMEOUT   (0)   /* NO PREPOST TIMEOUT => NO CPING/CPONG    */
#define AJP_DEF_RECOVERY_OPTS     (0)   /* NO RECOVERY / NO    */
#define AJP_DEF_SOCKET_TIMEOUT    (0)   /* No timeout */
#define AJP_DEF_PING_TIMEOUT      (10000)  /* Default CPING/CPONG timeout (10 seconds) */

#define AJP_CPING_NONE            (0)   /* Do not send cping packets */
#define AJP_CPING_CONNECT         (1)   /* Send cping on fresh connection */
#define AJP_CPING_PREPOST         (2)   /* Send cping before sending request */
#define AJP_CPING_INTERVAL        (4)   /* Send cping on regular intervals */


#define RECOVER_ABORT_IF_TCGETREQUEST    0x0001 /* DON'T RECOVER IF TOMCAT FAILS AFTER RECEIVING REQUEST */
#define RECOVER_ABORT_IF_TCSENDHEADER    0x0002 /* DON'T RECOVER IF TOMCAT FAILS AFTER SENDING HEADERS */
#define RECOVER_ABORT_IF_CLIENTERROR     0x0004 /* CLOSE THE SOCKET IN CASE OF CLIENT ERROR */
#define RECOVER_ALWAYS_HTTP_HEAD         0x0008 /* RECOVER HTTP HEAD REQUESTS, EVEN IF ABORT OPTIONS ARE SET */
#define RECOVER_ALWAYS_HTTP_GET          0x0010 /* RECOVER HTTP GET REQUESTS, EVEN IF ABORT OPTIONS ARE SET */

#define JK_MAX_HTTP_STATUS_FAILS   32   /* Should be enough for most 400 and 500 statuses */

struct jk_res_data
{
    int status;
#ifdef AS400
    char *msg;
#else
    const char *msg;
#endif
    unsigned num_headers;
    char **header_names;
    char **header_values;
};
typedef struct jk_res_data jk_res_data_t;

#include "jk_ajp14.h"

struct ajp_operation;
typedef struct ajp_operation ajp_operation_t;

struct ajp_endpoint;
typedef struct ajp_endpoint ajp_endpoint_t;

struct ajp_worker;
typedef struct ajp_worker ajp_worker_t;

struct ajp_worker
{
    jk_worker_t worker;
    /* Shared memory worker data */
    jk_shm_ajp_worker_t *s;

    char         name[JK_SHM_STR_SIZ+1];
    /* Sequence counter starting at 0 and increasing
     * every time we change the config
     */
    volatile unsigned int sequence;

    jk_pool_t p;
    jk_pool_atom_t buf[TINY_POOL_SIZE];

    JK_CRIT_SEC cs;

    struct sockaddr_in worker_inet_addr;    /* Contains host and port */
    unsigned connect_retry_attempts;
    char host[JK_SHM_STR_SIZ+1];
    int port;
    int addr_sequence;  /* Whether the address is resolved */
    int maintain_time;
    /*
     * Open connections cache...
     *
     * 1. Critical section object to protect the cache.
     * 2. Cache size.
     * 3. An array of "open" endpoints.
     */
    unsigned int ep_cache_sz;
    unsigned int ep_mincache_sz;
    unsigned int ep_maxcache_sz;
    int cache_acquire_timeout;
    ajp_endpoint_t **ep_cache;

    int proto;              /* PROTOCOL USED AJP13/AJP14 */

    jk_login_service_t *login;

    /* Weak secret similar with ajp12, used in ajp13 */
    const char *secret;

    /*
     * Post physical connect handler.
     * AJP14 will set here its login handler
     */
    int (*logon) (ajp_endpoint_t * ae, jk_logger_t *l);

    /*
     * Handle Socket Timeouts
     */
    int socket_timeout;
    int socket_connect_timeout;
    int keepalive;
    int socket_buf;
    /*
     * Handle Cache Timeouts
     */
    int cache_timeout;

    /*
     * Handle Connection/Reply Timeouts
     */
    int connect_timeout;      /* connect cping/cpong delay in ms (0 means disabled)  */
    int reply_timeout;        /* reply timeout delay in ms (0 means disabled) */
    int prepost_timeout;      /* before sending a request cping/cpong timeout delay in ms (0 means disabled) */
    int conn_ping_interval;   /* interval for sending keepalive cping packets on
                               * unused connection */
    int ping_timeout;         /* generic cping/cpong timeout. Used for keepalive packets or
                               * as default for boolean valued connect and prepost timeouts.
                               */
    unsigned int ping_mode;   /* Ping mode flags (which types of cpings should be used) */
    /*
     * Recovery options
     */
    unsigned int recovery_opts;

    /*
     * Public property to enable the number of retry attempts
     * on this worker.
     */
    int retries;

    unsigned int max_packet_size;  /*  Maximum AJP Packet size */

    int retry_interval;            /*  Number of milliseconds to sleep before doing a retry */

    /* 
     * HTTP status that will cause failover (0 means disabled)
     */
     unsigned int http_status_fail_num;
     int http_status_fail[JK_MAX_HTTP_STATUS_FAILS];
};


/*
 * endpoint, the remote connector which does the work
 */
struct ajp_endpoint
{
    ajp_worker_t *worker;

    jk_pool_t pool;
    jk_pool_atom_t buf[BIG_POOL_SIZE];

    int proto;              /* PROTOCOL USED AJP13/AJP14 */

    jk_sock_t sd;
    int reuse;

    jk_endpoint_t endpoint;

    jk_uint64_t left_bytes_to_send;

    /* time of the last request
       handled by this endpoint */
    time_t last_access;
    int last_errno;
    /* Last operation performed via this endpoint */
    int last_op;
};

/*
 * little struct to avoid multiples ptr passing
 * this struct is ready to hold upload file fd
 * to add upload persistant storage
 */
struct ajp_operation
{
    jk_msg_buf_t *request;  /* original request storage */
    jk_msg_buf_t *reply;    /* reply storage (chuncked by ajp13 */
    jk_msg_buf_t *post;     /* small post data storage area */
    int uploadfd;           /* future persistant storage id */
    int recoverable;        /* if exchange could be conducted on another TC */
};

/*
 * Functions
 */


const char *jk_ajp_get_state(ajp_worker_t *aw, jk_logger_t *l);

int jk_ajp_get_state_code(const char *v);

int ajp_validate(jk_worker_t *pThis,
                 jk_map_t *props,
                 jk_worker_env_t *we, jk_logger_t *l, int proto);

int ajp_init(jk_worker_t *pThis,
             jk_map_t *props,
             jk_worker_env_t *we, jk_logger_t *l, int proto);

int JK_METHOD ajp_worker_factory(jk_worker_t **w,
                                 const char *name, jk_logger_t *l);

int ajp_destroy(jk_worker_t **pThis, jk_logger_t *l, int proto);

int JK_METHOD ajp_done(jk_endpoint_t **e, jk_logger_t *l);

int ajp_get_endpoint(jk_worker_t *pThis,
                     jk_endpoint_t **pend, jk_logger_t *l, int proto);

int ajp_connect_to_endpoint(ajp_endpoint_t * ae, jk_logger_t *l);

void ajp_close_endpoint(ajp_endpoint_t * ae, jk_logger_t *l);

void jk_ajp_pull(ajp_worker_t * aw, int locked, jk_logger_t *l);

void jk_ajp_push(ajp_worker_t * aw, int locked, jk_logger_t *l);

int ajp_connection_tcp_send_message(ajp_endpoint_t * ae,
                                    jk_msg_buf_t *msg, jk_logger_t *l);

int ajp_connection_tcp_get_message(ajp_endpoint_t * ae,
                                   jk_msg_buf_t *msg, jk_logger_t *l);

int JK_METHOD ajp_maintain(jk_worker_t *pThis, time_t now, jk_logger_t *l);

int jk_ajp_get_cping_mode(const char *m, int def);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* JK_AJP_COMMON_H */
