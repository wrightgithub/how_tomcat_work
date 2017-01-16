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
 * Description: ajpv1.2 worker header file                                 *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Version:     $Revision: 466585 $                                           *
 ***************************************************************************/

#ifndef JK_AJP12_WORKER_H
#define JK_AJP12_WORKER_H

#include "jk_logger.h"
#include "jk_service.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define JK_AJP12_WORKER_NAME ("ajp12")
#define JK_AJP12_WORKER_TYPE (1)

int JK_METHOD ajp12_worker_factory(jk_worker_t **w,
                                   const char *name, jk_logger_t *l);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */

#endif                          /* JK_AJP12_WORKER_H */
