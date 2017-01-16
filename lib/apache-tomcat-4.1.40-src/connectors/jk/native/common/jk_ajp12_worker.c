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
 * Description: ajpv1.2 worker, used to call local or remote jserv hosts   *
 *              This worker is deprecated                                  *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Based on:    jserv_ajpv12.c from Jserv                                  *
 * Version:     $Revision: 747878 $                                          *
 ***************************************************************************/

#include "jk_ajp12_worker.h"
#include "jk_pool.h"
#include "jk_connect.h"
#include "jk_util.h"
#include "jk_sockbuf.h"
#if defined(AS400) && !defined(AS400_UTF8)
#include "util_ebcdic.h"
#include <string.h>
#endif

#define AJP_DEF_HOST            ("localhost")
#define AJP_DEF_PORT            (8007)
#define READ_BUF_SIZE           (8*1024)
#define DEF_RETRY_ATTEMPTS      (1)

struct ajp12_worker
{
    struct sockaddr_in worker_inet_addr;
    unsigned connect_retry_attempts;
    char *name;
    jk_worker_t worker;
};

typedef struct ajp12_worker ajp12_worker_t;

struct ajp12_endpoint
{
    ajp12_worker_t *worker;

    jk_sock_t sd;
    jk_sockbuf_t sb;

    jk_endpoint_t endpoint;
};
typedef struct ajp12_endpoint ajp12_endpoint_t;

static int ajpv12_mark(ajp12_endpoint_t * p, unsigned char type);

#if defined(AS400) && !defined(AS400_UTF8)
static int ajpv12_sendasciistring(ajp12_endpoint_t * p, char *buffer);
#endif

#if defined(AS400) && !defined(AS400_UTF8)
static int ajpv12_sendstring(ajp12_endpoint_t * p, char *buffer);
#else
static int ajpv12_sendstring(ajp12_endpoint_t * p, const char *buffer);
#endif

static int ajpv12_sendint(ajp12_endpoint_t * p, int d);

static int ajpv12_sendnbytes(ajp12_endpoint_t * p,
                             const void *buffer, int bufferlen);

static int ajpv12_flush(ajp12_endpoint_t * p);

static int ajpv12_handle_response(ajp12_endpoint_t * p,
                                  jk_ws_service_t *s, jk_logger_t *l);

static int ajpv12_handle_request(ajp12_endpoint_t * p,
                                 jk_ws_service_t *s, jk_logger_t *l);

/*
 * Return values of service() method for ajp12 worker:
 * return value  is_error              reason
 * JK_FALSE      JK_HTTP_SERVER_ERROR  Invalid parameters (null values)
 *                                     Error during connect to the backend
 *                                     ajpv12_handle_request() returns false:
 *           Any error during reading a request body from the client or
 *           sending the request to the backend
 * JK_FALSE      JK_HTTP_OK            ajpv12_handle_response() returns false:
 *           Any error during reading parts of response from backend or
 *           sending to client
 * JK_TRUE       JK_HTTP_OK            All other cases
 */
static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l, int *is_error)
{
    ajp12_endpoint_t *p;
    unsigned int attempt;
    int rc = -1;
    /*
     * AJP12 protocol is not recoverable.
     */

    JK_TRACE_ENTER(l);

    if (!e || !e->endpoint_private || !s || !is_error) {
        JK_LOG_NULL_PARAMS(l);
        if (is_error)
            *is_error = JK_HTTP_SERVER_ERROR;
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    p = e->endpoint_private;

    /* Set returned error to OK */
    *is_error = JK_HTTP_OK;

    for (attempt = 0; attempt < p->worker->connect_retry_attempts;
         attempt++) {
        p->sd =
            jk_open_socket(&p->worker->worker_inet_addr,
                           JK_FALSE, 0, 0, 0, l);

        jk_log(l, JK_LOG_DEBUG, "In jk_endpoint_t::service, sd = %d",
               p->sd);
        if (IS_VALID_SOCKET(p->sd)) {
            break;
        }
    }
    if (IS_VALID_SOCKET(p->sd)) {

        jk_sb_open(&p->sb, p->sd);
        if (ajpv12_handle_request(p, s, l)) {
            jk_log(l, JK_LOG_DEBUG,
                   "In jk_endpoint_t::service, sent request");
            rc = ajpv12_handle_response(p, s, l);
            JK_TRACE_EXIT(l);
            return rc;
        }
    }
    jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::service, Error sd = %d",
           p->sd);
    *is_error = JK_HTTP_SERVER_ERROR;

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD done(jk_endpoint_t **e, jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_endpoint_t::done");
    if (e && *e && (*e)->endpoint_private) {
        ajp12_endpoint_t *p = (*e)->endpoint_private;
        if (IS_VALID_SOCKET(p->sd)) {
            jk_shutdown_socket(p->sd, l);
        }
        free(p);
        *e = NULL;
        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, "In jk_endpoint_t::done, NULL parameters");
    return JK_FALSE;
}

static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,
                              jk_worker_env_t *we, jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::validate");

    if (pThis && pThis->worker_private) {
        ajp12_worker_t *p = pThis->worker_private;
        int port = jk_get_worker_port(props,
                                      p->name,
                                      AJP_DEF_PORT);

        const char *host = jk_get_worker_host(props,
                                        p->name,
                                        AJP_DEF_HOST);

        jk_log(l, JK_LOG_DEBUG,
               "In jk_worker_t::validate for worker %s contact is %s:%d",
               p->name, host, port);

        if (host) {
            if (jk_resolve(host, port, &p->worker_inet_addr, we->pool, l)) {
                return JK_TRUE;
            }
            jk_log(l, JK_LOG_ERROR,
                   "In jk_worker_t::validate, resolve failed");
        }
        jk_log(l, JK_LOG_ERROR, "In jk_worker_t::validate, Error %s %d",
               host, port);
    }
    else {
        jk_log(l, JK_LOG_ERROR,
               "In jk_worker_t::validate, NULL parameters");
    }

    return JK_FALSE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props,
                          jk_worker_env_t *we, jk_logger_t *log)
{
    /* Nothing to do for now */
    return JK_TRUE;
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend, jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::get_endpoint");

    if (pThis && pThis->worker_private && pend) {
        ajp12_endpoint_t *p =
            (ajp12_endpoint_t *) malloc(sizeof(ajp12_endpoint_t));
        if (p) {
            p->sd = JK_INVALID_SOCKET;
            p->worker = pThis->worker_private;
            p->endpoint.endpoint_private = p;
            p->endpoint.service = service;
            p->endpoint.done = done;
            *pend = &p->endpoint;
            return JK_TRUE;
        }
        jk_log(l, JK_LOG_ERROR,
               "In jk_worker_t::get_endpoint, malloc failed");
    }
    else {
        jk_log(l, JK_LOG_ERROR,
               "In jk_worker_t::get_endpoint, NULL parameters");
    }

    return JK_FALSE;
}

static int JK_METHOD destroy(jk_worker_t **pThis, jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into jk_worker_t::destroy");
    if (pThis && *pThis && (*pThis)->worker_private) {
        ajp12_worker_t *private_data = (*pThis)->worker_private;
        free(private_data->name);
        free(private_data);

        return JK_TRUE;
    }

    jk_log(l, JK_LOG_ERROR, "In jk_worker_t::destroy, NULL parameters");
    return JK_FALSE;
}

int JK_METHOD ajp12_worker_factory(jk_worker_t **w,
                                   const char *name, jk_logger_t *l)
{
    jk_log(l, JK_LOG_DEBUG, "Into ajp12_worker_factory");
    if (NULL != name && NULL != w) {
        ajp12_worker_t *private_data =
            (ajp12_worker_t *) malloc(sizeof(ajp12_worker_t));

        if (private_data) {
            private_data->name = strdup(name);

            if (private_data->name) {
                private_data->connect_retry_attempts = DEF_RETRY_ATTEMPTS;
                private_data->worker.worker_private = private_data;

                private_data->worker.validate = validate;
                private_data->worker.init = init;
                private_data->worker.get_endpoint = get_endpoint;
                private_data->worker.destroy = destroy;
                private_data->worker.maintain = NULL;

                *w = &private_data->worker;
                return JK_AJP12_WORKER_TYPE;
            }

            free(private_data);
        }
        jk_log(l, JK_LOG_ERROR, "In ajp12_worker_factory, malloc failed");
    }
    else {
        jk_log(l, JK_LOG_ERROR, "In ajp12_worker_factory, NULL parameters");
    }

    return 0;
}

static int ajpv12_sendnbytes(ajp12_endpoint_t * p,
                             const void *buffer, int bufferlen)
{
    unsigned char bytes[2];
    static const unsigned char null_b[2] =
        { (unsigned char)0xff, (unsigned char)0xff };

    if (buffer) {
        bytes[0] = (unsigned char)((bufferlen >> 8) & 0xff);
        bytes[1] = (unsigned char)(bufferlen & 0xff);

        if (jk_sb_write(&p->sb, bytes, 2)) {
            return jk_sb_write(&p->sb, buffer, bufferlen);
        }
        else {
            return JK_FALSE;
        }
    }
    else {
        return jk_sb_write(&p->sb, null_b, 2);
    }
}

#if defined(AS400) && !defined(AS400_UTF8)
static int ajpv12_sendasciistring(ajp12_endpoint_t * p, const char *buffer)
{
    int bufferlen;

    if (buffer && (bufferlen = strlen(buffer))) {
        return ajpv12_sendnbytes(p, buffer, bufferlen);
    }
    else {
        return ajpv12_sendnbytes(p, NULL, 0);
    }
}
#endif

static int ajpv12_sendstring(ajp12_endpoint_t * p, const char *buffer)
{
    int bufferlen;

    if (buffer && (bufferlen = (int)strlen(buffer))) {
#if (defined(AS400) && !defined(AS400_UTF8)) || defined(_OSD_POSIX)
        char buf[2048];
        if (bufferlen < 2048) {
            memcpy(buf, buffer, bufferlen);
            jk_xlate_to_ascii(buf, bufferlen);
            return ajpv12_sendnbytes(p, buf, bufferlen);
        }
        else
            return -1;
#else
        return ajpv12_sendnbytes(p, buffer, bufferlen);
#endif
    }
    else {
        return ajpv12_sendnbytes(p, NULL, 0);
    }
}

static int ajpv12_mark(ajp12_endpoint_t * p, unsigned char type)
{
    if (jk_sb_write(&p->sb, &type, 1)) {
        return JK_TRUE;
    }
    else {
        return JK_FALSE;
    }
}

static int ajpv12_sendint(ajp12_endpoint_t * p, int d)
{
    char buf[20];
    sprintf(buf, "%d", d);
    return ajpv12_sendstring(p, buf);
}

static int ajpv12_flush(ajp12_endpoint_t * p)
{
    return jk_sb_flush(&p->sb);
}

static int ajpv12_handle_request(ajp12_endpoint_t * p,
                                 jk_ws_service_t *s, jk_logger_t *l)
{
    int ret;

    jk_log(l, JK_LOG_DEBUG, "Into ajpv12_handle_request");
    /*
     * Start the ajp 12 service sequence
     */
    jk_log(l, JK_LOG_DEBUG,
           "ajpv12_handle_request, sending the ajp12 start sequence");

    ret = (ajpv12_mark(p, 1) && ajpv12_sendstring(p, s->method) && ajpv12_sendstring(p, 0) &&   /* zone */
           ajpv12_sendstring(p, 0) &&   /* servlet */
           ajpv12_sendstring(p, s->server_name) && ajpv12_sendstring(p, 0) &&   /* doc root */
           ajpv12_sendstring(p, 0) &&   /* path info */
           ajpv12_sendstring(p, 0) &&   /* path translated */
#if defined(AS400) && !defined(AS400_UTF8)
           ajpv12_sendasciistring(p, s->query_string) &&
#else
           ajpv12_sendstring(p, s->query_string) &&
#endif
           ajpv12_sendstring(p, s->remote_addr) &&
           ajpv12_sendstring(p, s->remote_host) &&
           ajpv12_sendstring(p, s->remote_user) &&
           ajpv12_sendstring(p, s->auth_type) &&
           ajpv12_sendint(p, s->server_port) &&
#if defined(AS400) && !defined(AS400_UTF8)
           ajpv12_sendasciistring(p, s->method) &&
#else
           ajpv12_sendstring(p, s->method) &&
#endif
           ajpv12_sendstring(p, s->req_uri) && ajpv12_sendstring(p, 0) &&       /* */
           ajpv12_sendstring(p, 0) &&   /* SCRIPT_NAME */
#if defined(AS400) && !defined(AS400_UTF8)
           ajpv12_sendasciistring(p, s->server_name) &&
#else
           ajpv12_sendstring(p, s->server_name) &&
#endif
           ajpv12_sendint(p, s->server_port) && ajpv12_sendstring(p, s->protocol) && ajpv12_sendstring(p, 0) && /* SERVER_SIGNATURE */
           ajpv12_sendstring(p, s->server_software) && ajpv12_sendstring(p, s->route) &&    /* JSERV_ROUTE */
           ajpv12_sendstring(p, "") &&  /* JSERV ajpv12 compatibility */
           ajpv12_sendstring(p, ""));   /* JSERV ajpv12 compatibility */

    if (!ret) {
        jk_log(l, JK_LOG_ERROR,
               "In ajpv12_handle_request, failed to send the ajp12 start sequence");
        return JK_FALSE;
    }

    if (s->num_attributes > 0) {
        unsigned i;
        jk_log(l, JK_LOG_DEBUG,
               "ajpv12_handle_request, sending the environment variables");

        for (i = 0; i < s->num_attributes; i++) {
            ret = (ajpv12_mark(p, 5) &&
                   ajpv12_sendstring(p, s->attributes_names[i]) &&
                   ajpv12_sendstring(p, s->attributes_values[i]));
            if (!ret) {
                jk_log(l, JK_LOG_ERROR,
                       "In ajpv12_handle_request, failed to send environment");
                return JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request, sending the headers");

    /* Send the request headers */
    if (s->num_headers) {
        unsigned i;
        for (i = 0; i < s->num_headers; ++i) {
            ret = (ajpv12_mark(p, 3) &&
                   ajpv12_sendstring(p, s->headers_names[i]) &&
                   ajpv12_sendstring(p, s->headers_values[i]));

            if (!ret) {
                jk_log(l, JK_LOG_ERROR,
                       "In ajpv12_handle_request, failed to send headers");
                return JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_DEBUG,
           "ajpv12_handle_request, sending the terminating mark");

    ret = (ajpv12_mark(p, 4) && ajpv12_flush(p));
    if (!ret) {
        jk_log(l, JK_LOG_ERROR,
               "In ajpv12_handle_request, failed to send the terminating mark");
        return JK_FALSE;
    }

    if (s->content_length) {
        char buf[READ_BUF_SIZE];
        jk_uint64_t so_far = 0;

        jk_log(l, JK_LOG_DEBUG,
               "ajpv12_handle_request, sending the request body");

        while (so_far < s->content_length) {
            unsigned this_time = 0;
            unsigned to_read;
            if (s->content_length > so_far + READ_BUF_SIZE) {
                to_read = READ_BUF_SIZE;
            }
            else {
                to_read = (unsigned int)(s->content_length - so_far);
            }

            if (!s->read(s, buf, to_read, &this_time)) {
                jk_log(l, JK_LOG_ERROR,
                       "In ajpv12_handle_request, failed to read from the web server");
                return JK_FALSE;
            }
            jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request, read %d bytes",
                   this_time);
            if (this_time > 0) {
                so_far += this_time;
                if ((int)this_time != send(p->sd, buf, this_time, 0)) {
                    jk_log(l, JK_LOG_ERROR,
                           "In ajpv12_handle_request, failed to write to the container");
                    return JK_FALSE;
                }
                jk_log(l, JK_LOG_DEBUG,
                       "ajpv12_handle_request, sent %d bytes", this_time);
            }
            else if (this_time == 0) {
                jk_log(l, JK_LOG_ERROR,
                       "In ajpv12_handle_request, Error: short read. content length is %" JK_UINT64_T_FMT ", read %" JK_UINT64_T_FMT,
                       s->content_length, so_far);
                return JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_request done");
    return JK_TRUE;
}

static int ajpv12_handle_response(ajp12_endpoint_t * p,
                                  jk_ws_service_t *s, jk_logger_t *l)
{
    int status = 200;
    char *reason = NULL;
    char **names = NULL;
    char **values = NULL;
    int headers_capacity = 0;
    int headers_len = 0;
    int write_to_ws;

    jk_log(l, JK_LOG_DEBUG, "Into ajpv12_handle_response");
    /*
     * Read headers ...
     */
    while (1) {
        char *line = NULL;
        char *name = NULL;
        char *value = NULL;
#ifdef _MT_CODE_PTHREAD
        char *lasts;
#endif

        if (!jk_sb_gets(&p->sb, &line)) {
            jk_log(l, JK_LOG_ERROR,
                   "ajpv12_handle_response, error reading header line");
            return JK_FALSE;
        }
#if (defined(AS400) && !defined(AS400_UTF8)) || defined(_OSD_POSIX)
        jk_xlate_from_ascii(line, strlen(line));
#endif

        jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, read %s", line);
        if (0 == strlen(line)) {
            jk_log(l, JK_LOG_DEBUG,
                   "ajpv12_handle_response, headers are done");
            break;              /* Empty line -> end of headers */
        }

        name = line;
        while (isspace((int)(*name)) && *name) {
            name++;             /* Skip leading white chars */
        }
        if (!*name) {           /* Empty header name */
            jk_log(l, JK_LOG_ERROR,
                   "ajpv12_handle_response, empty header name");
            return JK_FALSE;
        }
        if (!(value = strchr(name, ':'))) {
            jk_log(l, JK_LOG_ERROR,
                   "ajpv12_handle_response, no value supplied");
            return JK_FALSE;    /* No value !!! */
        }
        *value = '\0';
        value++;
        while (isspace((int)(*value)) && *value) {
            value++;            /* Skip leading white chars */
        }
        if (!*value) {          /* Empty header value */
            jk_log(l, JK_LOG_ERROR,
                   "ajpv12_handle_response, empty header value");
            return JK_FALSE;
        }

        jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, read %s=%s", name,
               value);
        if (0 == strcmp("Status", name)) {
#ifdef _MT_CODE_PTHREAD
            char *numeric = strtok_r(value, " \t", &lasts);
#else
            char *numeric = strtok(value, " \t");
#endif

            status = atoi(numeric);
            if (status < 100 || status > 999) {
                jk_log(l, JK_LOG_ERROR,
                       "ajpv12_handle_response, invalid status code");
                return JK_FALSE;
            }
#ifdef _MT_CODE_PTHREAD
            reason = jk_pool_strdup(s->pool, strtok_r(NULL, " \t", &lasts));
#else
            reason = jk_pool_strdup(s->pool, strtok(NULL, " \t"));
#endif
        }
        else {
            if (headers_capacity == headers_len) {
                jk_log(l, JK_LOG_DEBUG,
                       "ajpv12_handle_response, allocating header arrays");
                names =
                    (char **)jk_pool_realloc(s->pool,
                                             sizeof(char *) *
                                             (headers_capacity + 5), names,
                                             sizeof(char *) *
                                             headers_capacity);
                values =
                    (char **)jk_pool_realloc(s->pool,
                                             sizeof(char *) *
                                             (headers_capacity + 5), values,
                                             sizeof(char *) *
                                             headers_capacity);
                if (!values || !names) {
                    jk_log(l, JK_LOG_ERROR,
                           "ajpv12_handle_response, malloc error");
                    return JK_FALSE;
                }
                headers_capacity = headers_capacity + 5;
            }
            names[headers_len] = jk_pool_strdup(s->pool, name);
            values[headers_len] = jk_pool_strdup(s->pool, value);
            headers_len++;
        }
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response, starting response");
    if (!s->start_response(s,
                           status,
                           reason,
                           (const char *const *)names,
                           (const char *const *)values, headers_len)) {
        jk_log(l, JK_LOG_ERROR,
               "ajpv12_handle_response, error starting response");
        return JK_FALSE;
    }

    jk_log(l, JK_LOG_DEBUG,
           "ajpv12_handle_response, reading response body");
    /*
     * Read response body
     */
    write_to_ws = JK_TRUE;
    while (1) {
        unsigned to_read = READ_BUF_SIZE;
        unsigned acc = 0;
        char *buf = NULL;

        if (!jk_sb_read(&p->sb, &buf, to_read, &acc)) {
            jk_log(l, JK_LOG_ERROR,
                   "ajpv12_handle_response, error reading from ");
            return JK_FALSE;
        }

        if (!acc) {
            jk_log(l, JK_LOG_DEBUG,
                   "ajpv12_handle_response, response body is done");
            break;
        }

        if (write_to_ws) {
            if (!s->write(s, buf, acc)) {
                jk_log(l, JK_LOG_ERROR,
                       "ajpv12_handle_response, error writing back to server");
                write_to_ws = JK_FALSE;
            }
        }
    }

    jk_log(l, JK_LOG_DEBUG, "ajpv12_handle_response done");
    return JK_TRUE;
}
