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
 * Description: NSAPI plugin for Netscape servers                          *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision: 756058 $                                           *
 ***************************************************************************/


#include "nsapi.h"
#include "jk_global.h"
#include "jk_url.h"
#include "jk_util.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"
#include "jk_worker.h"
#include "jk_shm.h"
#include "jk_ajp13.h"

#define URI_PATTERN "path"
#define DEFAULT_WORKER_NAME ("ajp13")
#define REJECT_UNSAFE_TAG   "reject_unsafe"

#define STRNULL_FOR_NULL(x) ((x) ? (x) : "(null)")

struct nsapi_private_data
{
    jk_pool_t p;

    pblock *pb;
    Session *sn;
    Request *rq;
};
typedef struct nsapi_private_data nsapi_private_data_t;

static int init_on_other_thread_is_done = JK_FALSE;
static int init_on_other_thread_is_ok = JK_FALSE;

static const char ssl_cert_start[] = "-----BEGIN CERTIFICATE-----\r\n";
static const char ssl_cert_end[] = "\r\n-----END CERTIFICATE-----\r\n";

static jk_logger_t *logger = NULL;
static jk_worker_env_t worker_env;
static jk_map_t *init_map = NULL;
static jk_uri_worker_map_t *uw_map = NULL;
static size_t jk_shm_size = 0;

#ifdef NETWARE
int (*PR_IsSocketSecure) (SYS_NETFD * csd);     /* pointer to PR_IsSocketSecure function */
#endif

static int JK_METHOD start_response(jk_ws_service_t *s,
                                    int status,
                                    const char *reason,
                                    const char *const *header_names,
                                    const char *const *header_values,
                                    unsigned num_of_headers);

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned l, unsigned *a);

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned l);

NSAPI_PUBLIC int jk_init(pblock * pb, Session * sn, Request * rq);

NSAPI_PUBLIC void jk_term(void *p);

NSAPI_PUBLIC int jk_service(pblock * pb, Session * sn, Request * rq);

static int init_ws_service(nsapi_private_data_t * private_data,
                           jk_ws_service_t *s);

static int setup_http_headers(nsapi_private_data_t * private_data,
                              jk_ws_service_t *s);

static void init_workers_on_other_threads(void *init_d)
{
    init_map = (jk_map_t *)init_d;
    /* we add the URI->WORKER MAP since workers using AJP14 will feed it */
    /* but where are they here in Netscape ? */
    if (uri_worker_map_alloc(&uw_map, NULL, logger)) {
        uw_map->fname = "";
        uw_map->reload = JK_URIMAP_DEF_RELOAD;
        uw_map->reject_unsafe = jk_map_get_bool(init_map, "worker." REJECT_UNSAFE_TAG, JK_FALSE);
        worker_env.uri_to_worker = uw_map;
        worker_env.pool = NULL;

        if (wc_open(init_map, &worker_env, logger)) {
            init_on_other_thread_is_ok = JK_TRUE;
            uri_worker_map_ext(uw_map, logger);
            uri_worker_map_switch(uw_map, logger);
        }
        else {
            jk_log(logger, JK_LOG_EMERG,
                   "In init_workers_on_other_threads, failed");
        }
    }
    else {
        jk_log(logger, JK_LOG_EMERG,
               "In init_workers_on_other_threads, failed");
    }

    init_on_other_thread_is_done = JK_TRUE;
}

static int JK_METHOD start_response(jk_ws_service_t *s,
                                    int status,
                                    const char *reason,
                                    const char *const *header_names,
                                    const char *const *header_values,
                                    unsigned num_of_headers)
{
    if (s && s->ws_private) {
        nsapi_private_data_t *p = s->ws_private;
        if (!s->response_started) {
            unsigned i;

            s->response_started = JK_TRUE;

            /* Remove "old" content type */
            param_free(pblock_remove("content-type", p->rq->srvhdrs));

            if (num_of_headers) {
                for (i = 0; i < (int)num_of_headers; i++) {
                    pblock_nvinsert(header_names[i],
                                    header_values[i], p->rq->srvhdrs);
                }
            }
            else {
                pblock_nvinsert("content-type", "text/plain", p->rq->srvhdrs);
            }

            protocol_status(p->sn, p->rq, status, (char *)reason);

            protocol_start_response(p->sn, p->rq);
        }
        return JK_TRUE;

    }
    return JK_FALSE;
}

static int JK_METHOD ws_read(jk_ws_service_t *s,
                             void *b, unsigned l, unsigned *a)
{
    if (s && s->ws_private && b && a) {
        nsapi_private_data_t *p = s->ws_private;

        *a = 0;
        if (l) {
            unsigned i;
            netbuf *inbuf = p->sn->inbuf;

/* Until we get a service pack for NW5.1 and earlier that has the latest */
/* Enterprise Server, we have to go through the else version of this code*/
#if defined(netbuf_getbytes) && !defined(NETWARE)
            i = netbuf_getbytes(inbuf, b, l);
            if (NETBUF_EOF == i || NETBUF_ERROR == i) {
                return JK_FALSE;
            }

#else
            char *buf = b;
            int ch;
            for (i = 0; i < l; i++) {
                ch = netbuf_getc(inbuf);
                /*
                 * IO_EOF is 0 (zero) which is a very reasonable byte
                 * when it comes to binary data. So we are not breaking
                 * out of the read loop when reading it.
                 *
                 * We are protected from an infinit loop by the Java part of
                 * Tomcat.
                 */
                if (IO_ERROR == ch) {
                    break;
                }

                buf[i] = ch;
            }

            if (0 == i) {
                return JK_FALSE;
            }
#endif
            *a = i;

        }
        return JK_TRUE;
    }
    return JK_FALSE;
}

static int JK_METHOD ws_write(jk_ws_service_t *s, const void *b, unsigned l)
{
    if (s && s->ws_private && b) {
        nsapi_private_data_t *p = s->ws_private;

        if (l) {
            if (!s->response_started) {
                start_response(s, 200, NULL, NULL, NULL, 0);
            }

            if (net_write(p->sn->csd, (char *)b, (int)l) == IO_ERROR) {
                return JK_FALSE;
            }
        }

        return JK_TRUE;

    }
    return JK_FALSE;
}

NSAPI_PUBLIC int jk_init(pblock * pb, Session * sn, Request * rq)
{
    char *worker_prp_file = pblock_findval(JK_WORKER_FILE_TAG, pb);
    char *log_level_str = pblock_findval(JK_LOG_LEVEL_TAG, pb);
    char *log_file = pblock_findval(JK_LOG_FILE_TAG, pb);
    char *shm_file = pblock_findval(JK_SHM_FILE_TAG, pb);
    char *shm_file_safe = "";
    char *reject_unsafe = pblock_findval(REJECT_UNSAFE_TAG, pb);

    int rc = REQ_ABORTED;

    if (!worker_prp_file) {
        worker_prp_file = JK_WORKER_FILE_DEF;
    }

    if (!log_level_str) {
        log_level_str = JK_LOG_DEF_VERB;
    }

    if (!log_file) {
        fprintf(stderr,
                "Missing attribute %s in magnus.conf (jk_init) - aborting!\n", JK_LOG_FILE_TAG);
        return rc;
    }

    if (shm_file) {
        shm_file_safe = shm_file;
    }
#if !defined(WIN32) && !defined(NETWARE)
    else {
        fprintf(stderr,
                "Missing attribute %s in magnus.conf (jk_init) - aborting!\n", JK_SHM_FILE_TAG);
        return rc;
    }
#endif

    fprintf(stderr,
            "In jk_init.\n   Worker file = %s.\n   Log level = %s.\n   Log File = %s\n   SHM File = %s\n",
            worker_prp_file, log_level_str, log_file, shm_file);

    if (!jk_open_file_logger(&logger, log_file,
                             jk_parse_log_level(log_level_str))) {
        logger = NULL;
    }
    
    if (jk_map_alloc(&init_map)) {
        if (jk_map_read_properties(init_map, NULL, worker_prp_file, NULL,
                                   JK_MAP_HANDLE_DUPLICATES, logger)) {
            int rv;
            int sleep_cnt;
            SYS_THREAD s;

            if (jk_map_resolve_references(init_map, "worker.", 1, 1, logger) == JK_FALSE) {
                jk_log(logger, JK_LOG_ERROR, "Error in resolving configuration references");
            }

            if (reject_unsafe) {
                jk_map_add(init_map, "worker." REJECT_UNSAFE_TAG, reject_unsafe); 
            }

            jk_shm_size = jk_shm_calculate_size(init_map, logger);
            if ((rv = jk_shm_open(shm_file, jk_shm_size, logger)) != 0)
                jk_log(logger, JK_LOG_ERROR,
                       "Initializing shm:%s errno=%d. Load balancing workers will not function properly.",
                       jk_shm_name(), rv);

            s = systhread_start(SYSTHREAD_DEFAULT_PRIORITY,
                                0, init_workers_on_other_threads, init_map);
            for (sleep_cnt = 0; sleep_cnt < 60; sleep_cnt++) {
                systhread_sleep(1000);
                jk_log(logger, JK_LOG_DEBUG, "jk_init, a second passed");
                if (init_on_other_thread_is_done) {
                    break;
                }
            }

            if (init_on_other_thread_is_done && init_on_other_thread_is_ok) {
                magnus_atrestart(jk_term, NULL);
                rc = REQ_PROCEED;
                jk_log(logger, JK_LOG_INFO, "%s initialized", JK_EXPOSED_VERSION);
            }

/*            if(wc_open(init_map, NULL, logger)) {
                magnus_atrestart(jk_term, NULL);
                rc = REQ_PROCEED;
            }
*/
        }
    }

#ifdef NETWARE
    PR_IsSocketSecure =
        (int (*)(void **))ImportSymbol(GetNLMHandle(), "PR_IsSocketSecure");
#endif
    return rc;
}

NSAPI_PUBLIC void jk_term(void *p)
{
#ifdef NETWARE
    if (NULL != PR_IsSocketSecure) {
        UnimportSymbol(GetNLMHandle(), "PR_IsSocketSecure");
        PR_IsSocketSecure = NULL;
    }
#endif
    if (uw_map) {
        uri_worker_map_free(&uw_map, logger);
    }

    if (init_map) {
        jk_map_free(&init_map);
    }

    wc_close(logger);
    jk_shm_close();
    if (logger) {
        jk_close_file_logger(&logger);
    }
}

NSAPI_PUBLIC int jk_service(pblock * pb, Session * sn, Request * rq)
{
    char *worker_name = pblock_findval(JK_WORKER_NAME_TAG, pb);
    char *uri_pattern = pblock_findval(URI_PATTERN, pb);
    jk_worker_t *worker;
    int rc = REQ_ABORTED;

    if (uri_pattern) {
        char *uri = pblock_findval("uri", rq->reqpb);

        if (0 != shexp_match(uri, uri_pattern)) {
            return REQ_NOACTION;
        }
    }

    if (!worker_name) {
        worker_name = DEFAULT_WORKER_NAME;
    }

    worker = wc_get_worker_for_name(worker_name, logger);
    if (worker) {
        nsapi_private_data_t private_data;
        jk_ws_service_t s;
        jk_pool_atom_t buf[SMALL_POOL_SIZE];

        jk_open_pool(&private_data.p, buf, sizeof(buf));

        private_data.pb = pb;
        private_data.sn = sn;
        private_data.rq = rq;

        jk_init_ws_service(&s);

        s.ws_private = &private_data;
        s.pool = &private_data.p;

        wc_maintain(logger);
        if (init_ws_service(&private_data, &s)) {
            jk_endpoint_t *e = NULL;
            if (worker->get_endpoint(worker, &e, logger)) {
                int is_error = JK_HTTP_SERVER_ERROR;
                int result;
                if ((result = e->service(e, &s, logger, &is_error)) > 0) {
                    rc = REQ_PROCEED;
                    if (JK_IS_DEBUG_LEVEL(logger))
                        jk_log(logger, JK_LOG_DEBUG,
                               "service() returned OK");
                }
                else {
                    protocol_status(sn, rq, is_error, NULL);
                    if ((result == JK_CLIENT_ERROR) && (is_error == JK_HTTP_OK)) {
                        rc = REQ_EXIT;
                        jk_log(logger, JK_LOG_INFO,
                               "service() failed because client aborted connection");
                    }
                    else {
                        rc = REQ_ABORTED;
                        jk_log(logger, JK_LOG_ERROR,
                               "service() failed with http error %d", is_error);
                    }
                }

                e->done(&e, logger);
            }
        }
        jk_close_pool(&private_data.p);
    }

    return rc;
}

static int init_ws_service(nsapi_private_data_t * private_data,
                           jk_ws_service_t *s)
{
    char *tmp;
    int size;
    int rc;

    s->start_response = start_response;
    s->read = ws_read;
    s->write = ws_write;

    s->auth_type = pblock_findval("auth-type", private_data->rq->vars);
    s->remote_user = pblock_findval("auth-user", private_data->rq->vars);

    tmp = NULL;
    rc = request_header("content-length",
                        &tmp, private_data->sn, private_data->rq);

    if ((rc != REQ_ABORTED) && tmp) {
        sscanf(tmp, "%" JK_UINT64_T_FMT, &(s->content_length));
    }

    s->method = pblock_findval("method", private_data->rq->reqpb);
    s->protocol = pblock_findval("protocol", private_data->rq->reqpb);

    s->remote_host = session_dns(private_data->sn);
    s->remote_addr = pblock_findval("ip", private_data->sn->client);

    tmp = pblock_findval("uri", private_data->rq->reqpb);
    size = 3 * strlen(tmp) + 1;
    s->req_uri = jk_pool_alloc(s->pool, size);
    jk_canonenc(tmp, s->req_uri, size);

    s->query_string = pblock_findval("query", private_data->rq->reqpb);

    s->server_name = server_hostname;

#ifdef NETWARE
    /* On NetWare, since we have virtual servers, we have a different way of
     * getting the port that we need to try first.
     */
    tmp = pblock_findval("server_port", private_data->sn->client);
    if (NULL != tmp)
        s->server_port = atoi(tmp);
    else
#endif
        s->server_port = server_portnum;
    s->server_software = system_version();

    s->uw_map = uw_map;

#ifdef NETWARE
    /* on NetWare, we can have virtual servers that are secure.
     * PR_IsSocketSecure is an api made available with virtual servers to check
     * if the socket is secure or not
     */
    if (NULL != PR_IsSocketSecure)
        s->is_ssl = PR_IsSocketSecure(private_data->sn->csd);
    else
#endif
        s->is_ssl = security_active;

    if (s->is_ssl) {
        char *ssl_cert = pblock_findval("auth-cert", private_data->rq->vars);
        if (ssl_cert != NULL) {
            s->ssl_cert = jk_pool_alloc(s->pool, sizeof(ssl_cert_start)+
                                                 strlen(ssl_cert)+
                                                 sizeof(ssl_cert_end));
            strcpy(s->ssl_cert, ssl_cert_start);
            strcat(s->ssl_cert, ssl_cert);
            strcat(s->ssl_cert, ssl_cert_end);
            s->ssl_cert_len = strlen(s->ssl_cert);
        }
        s->ssl_cipher = pblock_findval("cipher", private_data->sn->client);
        s->ssl_session = pblock_findval("ssl-id", private_data->sn->client);
        /* XXX: We need to investigate how to set s->ssl_key_size */
    }

    rc = setup_http_headers(private_data, s);

    /* Dump all connection param so we can trace what's going to
     * the remote tomcat
     */
    if (JK_IS_DEBUG_LEVEL(logger)) {
        jk_log(logger, JK_LOG_DEBUG,
               "Service protocol=%s method=%s host=%s addr=%s name=%s port=%d auth=%s user=%s uri=%s",
               STRNULL_FOR_NULL(s->protocol),
               STRNULL_FOR_NULL(s->method),
               STRNULL_FOR_NULL(s->remote_host),
               STRNULL_FOR_NULL(s->remote_addr),
               STRNULL_FOR_NULL(s->server_name),
               s->server_port,
               STRNULL_FOR_NULL(s->auth_type),
               STRNULL_FOR_NULL(s->remote_user),
               STRNULL_FOR_NULL(s->req_uri));
    }

    return rc;

}

static int setup_http_headers(nsapi_private_data_t * private_data,
                              jk_ws_service_t *s)
{
    int need_content_length_header =
        (s->content_length == 0) ? JK_TRUE : JK_FALSE;

    pblock *headers_jar = private_data->rq->headers;
    int cnt;
    int i;

    for (i = 0, cnt = 0; i < headers_jar->hsize; i++) {
        struct pb_entry *h = headers_jar->ht[i];
        while (h && h->param) {
            cnt++;
            h = h->next;
        }
    }

    s->headers_names = NULL;
    s->headers_values = NULL;
    s->num_headers = cnt;
    if (cnt) {
        /* allocate an extra header slot in case we need to add a content-length header */
        s->headers_names =
            jk_pool_alloc(&private_data->p, (cnt + 1) * sizeof(char *));
        s->headers_values =
            jk_pool_alloc(&private_data->p, (cnt + 1) * sizeof(char *));

        if (s->headers_names && s->headers_values) {
            for (i = 0, cnt = 0; i < headers_jar->hsize; i++) {
                struct pb_entry *h = headers_jar->ht[i];
                while (h && h->param) {
                    s->headers_names[cnt] = h->param->name;
                    s->headers_values[cnt] = h->param->value;
                    if (need_content_length_header &&
                        !strncmp(h->param->name, "content-length", 14)) {
                        need_content_length_header = JK_FALSE;
                    }
                    cnt++;
                    h = h->next;
                }
            }
            /* Add a content-length = 0 header if needed.
             * Ajp13 assumes an absent content-length header means an unknown,
             * but non-zero length body.
             */
            if (need_content_length_header) {
                s->headers_names[cnt] = "content-length";
                s->headers_values[cnt] = "0";
                cnt++;
            }
            s->num_headers = cnt;
            return JK_TRUE;
        }
    }
    else {
        if (need_content_length_header) {
            s->headers_names =
                jk_pool_alloc(&private_data->p, sizeof(char *));
            s->headers_values =
                jk_pool_alloc(&private_data->p, sizeof(char *));
            if (s->headers_names && s->headers_values) {
                s->headers_names[0] = "content-length";
                s->headers_values[0] = "0";
                s->num_headers++;
                return JK_TRUE;
            }
        }
        else
            return JK_TRUE;
    }

    return JK_FALSE;
}
