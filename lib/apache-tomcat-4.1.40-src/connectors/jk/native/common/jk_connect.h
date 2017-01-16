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
 * Description: Socket connections header file                             *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision: 706039 $                                               *
 ***************************************************************************/

#ifndef JK_CONNECT_H
#define JK_CONNECT_H

#include "jk_logger.h"
#include "jk_global.h"

#if !defined(WIN32) && !(defined(NETWARE) && defined(__NOVELL_LIBC__))
#define closesocket         close
#endif

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define JK_SOCKET_EOF      (-2)

int jk_resolve(const char *host, int port, struct sockaddr_in *rc, void *pool, jk_logger_t *l);

jk_sock_t jk_open_socket(struct sockaddr_in *addr, int keepalive,
                         int timeout, int connect_timeout,
                         int sock_buf, jk_logger_t *l);

int jk_close_socket(jk_sock_t sd, jk_logger_t *l);

int jk_shutdown_socket(jk_sock_t sd, jk_logger_t *l);

int jk_tcp_socket_sendfull(jk_sock_t sd, const unsigned char *b, int len, jk_logger_t *l);

int jk_tcp_socket_recvfull(jk_sock_t sd, unsigned char *b, int len, jk_logger_t *l);

char *jk_dump_hinfo(struct sockaddr_in *saddr, char *buf);

int jk_is_input_event(jk_sock_t sd, int timeout, jk_logger_t *l);

int jk_is_socket_connected(jk_sock_t sd, jk_logger_t *l);


/***
 * i5/OS V5R4 need ASCII<->EBCDIC translation for inet_addr() call
 */
#if !defined(AS400_UTF8)

#define jk_inet_addr inet_addr

#endif


#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_CONNECT_H */
