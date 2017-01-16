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
 * Description: Various utility functions                                  *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Author:      Rainer Jung <rjung@apache.org>                             *
 * Version:     $Revision: 751914 $                                          *
 ***************************************************************************/
#ifndef _JK_UTIL_H
#define _JK_UTIL_H

#include "jk_global.h"
#include "jk_logger.h"
#include "jk_map.h"
#include "jk_pool.h"
#include "jk_service.h"

#define JK_SLEEP_DEF     (100)

const char *jk_get_bool(int v);

int jk_get_bool_code(const char *v, int def);

void jk_sleep(int ms);

void jk_set_time_fmt(jk_logger_t *l, const char *jk_log_fmt);

int jk_parse_log_level(const char *level);

int jk_open_file_logger(jk_logger_t **l, const char *file, int level);

int jk_attach_file_logger(jk_logger_t **l, int fd, int level);

int jk_close_file_logger(jk_logger_t **l);

int jk_log(jk_logger_t *l,
           const char *file, int line, const char *funcname, int level,
           const char *fmt, ...);

/* [V] Two general purpose functions. Should ease the function bloat. */
int jk_get_worker_str_prop(jk_map_t *m,
                           const char *wname, const char *pname, const char **prop);

int jk_get_worker_int_prop(jk_map_t *m,
                           const char *wname, const char *pname, int *prop);

const char *jk_get_worker_host(jk_map_t *m, const char *wname, const char *def);

const char *jk_get_worker_type(jk_map_t *m, const char *wname);

int jk_get_worker_port(jk_map_t *m, const char *wname, int def);

int jk_get_worker_cache_size(jk_map_t *m, const char *wname, int def);

int jk_get_worker_cache_size_min(jk_map_t *m, const char *wname, int def);

int jk_get_worker_cache_acquire_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_socket_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_socket_connect_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_socket_buffer(jk_map_t *m, const char *wname, int def);

int jk_get_worker_socket_keepalive(jk_map_t *m, const char *wname, int def);

int jk_get_worker_conn_ping_interval(jk_map_t *m, const char *wname, int def);

int jk_get_worker_cache_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_recovery_opts(jk_map_t *m, const char *wname, int def);

int jk_get_worker_connect_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_reply_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_prepost_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_ping_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_ping_mode(jk_map_t *m, const char *wname, int def);

int jk_get_worker_recycle_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_recover_timeout(jk_map_t *m, const char *wname, int def);

int jk_get_worker_error_escalation_time(jk_map_t *m, const char *wname, int def);

int jk_get_worker_max_reply_timeouts(jk_map_t *m, const char *wname, int def);

int jk_get_worker_retry_interval(jk_map_t *m, const char *wname, int def);

const char *jk_get_worker_route(jk_map_t *m, const char *wname, const char *def);

const char *jk_get_worker_domain(jk_map_t *m, const char *wname, const char *def);

const char *jk_get_worker_redirect(jk_map_t *m, const char *wname, const char *def);

const char *jk_get_worker_secret_key(jk_map_t *m, const char *wname);

const char *jk_get_lb_session_cookie(jk_map_t *m, const char *wname, const char *def);

const char *jk_get_lb_session_path(jk_map_t *m, const char *wname, const char *def);

int jk_get_worker_retries(jk_map_t *m, const char *wname, int def);

int jk_get_is_worker_disabled(jk_map_t *m, const char *wname);

int jk_get_is_worker_stopped(jk_map_t *m, const char *wname);

int jk_get_worker_activation(jk_map_t *m, const char *wname);

int jk_get_worker_list(jk_map_t *m, char ***list, unsigned *num_of_workers);

int jk_get_lb_factor(jk_map_t *m, const char *wname);

int jk_get_distance(jk_map_t *m, const char *wname);

int jk_get_is_sticky_session(jk_map_t *m, const char *wname);

int jk_get_is_sticky_session_force(jk_map_t *m, const char *wname);

int jk_get_lb_method(jk_map_t *m, const char *wname);

int jk_get_lb_lock(jk_map_t *m, const char *wname);

int jk_get_lb_worker_list(jk_map_t *m,
                          const char *lb_wname,
                          char ***list, unsigned int *num_of_workers);
int jk_get_worker_mount_list(jk_map_t *m,
                             const char *wname,
                             char ***list, unsigned int *num_of_maps);
const char *jk_get_worker_secret(jk_map_t *m, const char *wname);

int jk_get_worker_mx(jk_map_t *m, const char *wname, unsigned *mx);

int jk_get_worker_ms(jk_map_t *m, const char *wname, unsigned *ms);

int jk_get_worker_classpath(jk_map_t *m, const char *wname, const char **cp);


int jk_get_worker_bridge_type(jk_map_t *m, const char *wname, unsigned *bt);

int jk_get_worker_jvm_path(jk_map_t *m, const char *wname, const char **vm_path);

int jk_get_worker_callback_dll(jk_map_t *m,
                               const char *wname, const char **cb_path);

int jk_get_worker_cmd_line(jk_map_t *m, const char *wname, const char **cmd_line);

int jk_file_exists(const char *f);

int jk_is_list_property(const char *prp_name);

int jk_is_path_property(const char *prp_name);

int jk_is_cmd_line_property(const char *prp_name);

int jk_is_unique_property(const char *prp_name);

int jk_is_deprecated_property(const char *prp_name);

int jk_is_valid_property(const char *prp_name);

int jk_get_worker_stdout(jk_map_t *m, const char *wname, const char **stdout_name);

int jk_get_worker_stderr(jk_map_t *m, const char *wname, const char **stderr_name);

int jk_get_worker_sysprops(jk_map_t *m, const char *wname, const char **sysprops);

int jk_get_worker_libpath(jk_map_t *m, const char *wname, const char **libpath);

char **jk_parse_sysprops(jk_pool_t *p, const char *sysprops);


void jk_append_libpath(jk_pool_t *p, const char *libpath);

void jk_set_worker_def_cache_size(int sz);

int jk_get_worker_def_cache_size(int protocol);

int jk_get_worker_maintain_time(jk_map_t *m);

int jk_get_max_packet_size(jk_map_t *m, const char *wname);

const char *jk_get_worker_style_sheet(jk_map_t *m, const char *wname, const char *def);

int jk_get_is_read_only(jk_map_t *m, const char *wname);

int jk_get_worker_user_list(jk_map_t *m,
                            const char *wname,
                            char ***list, unsigned int *num);

int jk_get_worker_good_rating(jk_map_t *m,
                              const char *wname,
                              char ***list, unsigned int *num);

int jk_get_worker_bad_rating(jk_map_t *m,
                             const char *wname,
                             char ***list, unsigned int *num);

const char *jk_get_worker_name_space(jk_map_t *m, const char *wname, const char *def);

const char *jk_get_worker_xmlns(jk_map_t *m, const char *wname, const char *def);

const char *jk_get_worker_xml_doctype(jk_map_t *m, const char *wname, const char *def);

const char *jk_get_worker_prop_prefix(jk_map_t *m, const char *wname, const char *def);

int jk_get_worker_fail_on_status(jk_map_t *m, const char *wname,
                                 int *list, unsigned int list_size);

int jk_get_worker_user_case_insensitive(jk_map_t *m, const char *wname);

int is_http_status_fail(unsigned int http_status_fail_num,
                        int *http_status_fail, int status);

int jk_wildchar_match(const char *str, const char *exp, int icase);

#define TC32_BRIDGE_TYPE    32
#define TC33_BRIDGE_TYPE    33
#define TC40_BRIDGE_TYPE    40
#define TC41_BRIDGE_TYPE    41
#define TC50_BRIDGE_TYPE    50

#ifdef AS400

#define S_IFREG _S_IFREG

#ifdef AS400_UTF8

void jk_ascii2ebcdic(char *src, char *dst);
void jk_ebcdic2ascii(char *src, char *dst);

#endif /* AS400_UTF8 */

#endif

/* i5/OS V5R4 need ASCII-EBCDIC conversion before stat() call */
/* added a stat() mapper function, jk_stat, for such purpose */

int jk_stat(const char *f, struct stat * statbuf);

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */


#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _JK_UTIL_H */
