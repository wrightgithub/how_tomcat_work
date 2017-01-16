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
 * Description: Socket buffer header file                                  *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision: 466585 $                                           *
 ***************************************************************************/

#include "jk_global.h"

#define SOCKBUF_SIZE (8*1024)

struct jk_sockbuf
{
    char buf[SOCKBUF_SIZE];
    unsigned int start;
    unsigned int end;
    jk_sock_t sd;
};
typedef struct jk_sockbuf jk_sockbuf_t;

int jk_sb_open(jk_sockbuf_t *sb, jk_sock_t sd);

int jk_sb_write(jk_sockbuf_t *sb, const void *buf, unsigned sz);

int jk_sb_read(jk_sockbuf_t *sb, char **buf, unsigned sz, unsigned *ac);

int jk_sb_flush(jk_sockbuf_t *sb);

int jk_sb_gets(jk_sockbuf_t *sb, char **ps);
