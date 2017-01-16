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
 * Description: load balance worker header file                            *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Rainer Jung <rjung@apache.org>                             *
 * Version:     $Revision: 751916 $                                           *
 ***************************************************************************/

#ifndef JK_LB_WORKER_H
#define JK_LB_WORKER_H

#include "jk_logger.h"
#include "jk_service.h"
#include "jk_mt.h"
#include "jk_shm.h"

#ifdef __cplusplus
extern "C"
{
#endif                          /* __cplusplus */

#define JK_LB_WORKER_NAME     ("lb")
#define JK_LB_WORKER_TYPE     (5)
#define JK_LB_SUB_WORKER_TYPE (7)
#define JK_LB_DEF_DOMAIN_NAME ("unknown")

#define JK_LB_METHOD_REQUESTS          (0)
#define JK_LB_METHOD_TRAFFIC           (1)
#define JK_LB_METHOD_BUSYNESS          (2)
#define JK_LB_METHOD_SESSIONS          (3)
#define JK_LB_METHOD_DEF               (JK_LB_METHOD_REQUESTS)
#define JK_LB_METHOD_MAX               (JK_LB_METHOD_SESSIONS)
#define JK_LB_METHOD_TEXT_REQUESTS     ("Request")
#define JK_LB_METHOD_TEXT_TRAFFIC      ("Traffic")
#define JK_LB_METHOD_TEXT_BUSYNESS     ("Busyness")
#define JK_LB_METHOD_TEXT_SESSIONS     ("Sessions")
#define JK_LB_METHOD_TEXT_DEF          (JK_LB_METHOD_TEXT_REQUESTS)
#define JK_LB_LOCK_OPTIMISTIC          (0)
#define JK_LB_LOCK_PESSIMISTIC         (1)
#define JK_LB_LOCK_DEF                 (JK_LB_LOCK_OPTIMISTIC)
#define JK_LB_LOCK_MAX                 (JK_LB_LOCK_PESSIMISTIC)
#define JK_LB_LOCK_TEXT_OPTIMISTIC     ("Optimistic")
#define JK_LB_LOCK_TEXT_PESSIMISTIC    ("Pessimistic")
#define JK_LB_LOCK_TEXT_DEF            (JK_LB_LOCK_TEXT_OPTIMISTIC)
/*
 * The following definitions for state and activation
 * need to be kept in sync with the two macros 
 * JK_WORKER_USABLE() and JK_WORKER_USABLE_STICKY() in jk_lb_worker.c.
 * Since we use ordered comparisons there instead of multiple
 * equal/unequal compares, order of the values is critical here.
 */
#define JK_LB_STATE_IDLE               (0)
#define JK_LB_STATE_OK                 (1)
#define JK_LB_STATE_RECOVER            (2)
#define JK_LB_STATE_FORCE              (3)
#define JK_LB_STATE_BUSY               (4)
#define JK_LB_STATE_ERROR              (5)
#define JK_LB_STATE_PROBE              (6)
#define JK_LB_STATE_DEF                (JK_LB_STATE_IDLE)
#define JK_LB_STATE_TEXT_IDLE          ("OK/IDLE")
#define JK_LB_STATE_TEXT_OK            ("OK")
#define JK_LB_STATE_TEXT_RECOVER       ("ERR/REC")
#define JK_LB_STATE_TEXT_FORCE         ("ERR/FRC")
#define JK_LB_STATE_TEXT_BUSY          ("OK/BUSY")
#define JK_LB_STATE_TEXT_ERROR         ("ERR")
#define JK_LB_STATE_TEXT_PROBE         ("ERR/PRB")
#define JK_LB_STATE_TEXT_MAX           (JK_LB_STATE_PROBE)
#define JK_LB_STATE_TEXT_DEF           (JK_LB_STATE_TEXT_IDLE)
/* All JK_LB_ACTIVATION_* values must be single digit. */
/* Otherwise the string encoding of the activation array */
/* fails e.g. in the isapi redirector. */
/* JK_LB_ACTIVATION_UNSET is not allowed as an actual worker state. */
/* It will not work e.g. when the status worker tries to show the state. */
/* It is only used in rule extension data to indicate, that the */
/* activation state should not be overwritten. */
#define JK_LB_ACTIVATION_ACTIVE        (0)
#define JK_LB_ACTIVATION_DISABLED      (1)
#define JK_LB_ACTIVATION_STOPPED       (2)
#define JK_LB_ACTIVATION_UNSET         (9)
#define JK_LB_ACTIVATION_DEF           (JK_LB_ACTIVATION_ACTIVE)
#define JK_LB_ACTIVATION_MAX           (JK_LB_ACTIVATION_STOPPED)
#define JK_LB_ACTIVATION_TEXT_ACTIVE   ("ACT")
#define JK_LB_ACTIVATION_TEXT_DISABLED ("DIS")
#define JK_LB_ACTIVATION_TEXT_STOPPED  ("STP")
#define JK_LB_ACTIVATION_TEXT_DEF      (JK_LB_ACTIVATION_TEXT_ACTIVE)

#define JK_LB_UINT64_STR_SZ          (21)
#define JK_LB_NOTES_COUNT            (9)
#define JK_NOTE_LB_FIRST_NAME        ("JK_LB_FIRST_NAME")
#define JK_NOTE_LB_FIRST_VALUE       ("JK_LB_FIRST_VALUE")
#define JK_NOTE_LB_FIRST_ACCESSED    ("JK_LB_FIRST_ACCESSED")
#define JK_NOTE_LB_FIRST_READ        ("JK_LB_FIRST_READ")
#define JK_NOTE_LB_FIRST_TRANSFERRED ("JK_LB_FIRST_TRANSFERRED")
#define JK_NOTE_LB_FIRST_ERRORS      ("JK_LB_FIRST_ERRORS")
#define JK_NOTE_LB_FIRST_BUSY        ("JK_LB_FIRST_BUSY")
#define JK_NOTE_LB_FIRST_ACTIVATION  ("JK_LB_FIRST_ACTIVATION")
#define JK_NOTE_LB_FIRST_STATE       ("JK_LB_FIRST_STATE")
#define JK_NOTE_LB_LAST_NAME         ("JK_LB_LAST_NAME")
#define JK_NOTE_LB_LAST_VALUE        ("JK_LB_LAST_VALUE")
#define JK_NOTE_LB_LAST_ACCESSED     ("JK_LB_LAST_ACCESSED")
#define JK_NOTE_LB_LAST_READ         ("JK_LB_LAST_READ")
#define JK_NOTE_LB_LAST_TRANSFERRED  ("JK_LB_LAST_TRANSFERRED")
#define JK_NOTE_LB_LAST_ERRORS       ("JK_LB_LAST_ERRORS")
#define JK_NOTE_LB_LAST_BUSY         ("JK_LB_LAST_BUSY")
#define JK_NOTE_LB_LAST_ACTIVATION   ("JK_LB_LAST_ACTIVATION")
#define JK_NOTE_LB_LAST_STATE        ("JK_LB_LAST_STATE")

/* Time to wait before retry. */
#define WAIT_BEFORE_RECOVER   (60)
/* We accept doing global maintenance if we are */
/* JK_LB_MAINTAIN_TOLERANCE seconds early. */
#define JK_LB_MAINTAIN_TOLERANCE (2)
/* We divide load values by 2^x during global maintenance. */
/* The exponent x is JK_LB_DECAY_MULT*#MAINT_INTV_SINCE_LAST_MAINT */
#define JK_LB_DECAY_MULT         (1)

struct lb_sub_worker
{
    jk_worker_t *worker;
    /* Shared memory worker data */
    jk_shm_lb_sub_worker_t *s;

    char         name[JK_SHM_STR_SIZ+1];
    /* Sequence counter starting at 0 and increasing
     * every time we change the config
     */
    volatile unsigned int sequence;

    /* route */
    char    route[JK_SHM_STR_SIZ+1];
    /* worker domain */
    char    domain[JK_SHM_STR_SIZ+1];
    /* worker redirect route */
    char    redirect[JK_SHM_STR_SIZ+1];
    /* worker distance */
    int distance;
    /* current activation state (config) of the worker */
    int activation;
    /* Current lb factor */
    int lb_factor;
    /* Current worker index */
    int i;
    /* Current lb reciprocal factor */
    jk_uint64_t lb_mult;
};
typedef struct lb_sub_worker lb_sub_worker_t;

struct lb_worker
{
    jk_worker_t worker;
    /* Shared memory worker data */
    jk_shm_lb_worker_t *s;

    char         name[JK_SHM_STR_SIZ+1];
    /* Sequence counter starting at 0 and increasing
     * every time we change the config
     */
    volatile unsigned int sequence;

    jk_pool_t p;
    jk_pool_atom_t buf[TINY_POOL_SIZE];

    JK_CRIT_SEC cs;

    lb_sub_worker_t *lb_workers;
    unsigned int num_of_workers;
    int          sticky_session;
    int          sticky_session_force;
    int          recover_wait_time;
    int          error_escalation_time;
    int          max_reply_timeouts;
    int          retries;
    int          retry_interval;
    int          lbmethod;
    int          lblock;
    int          maintain_time;
    unsigned int max_packet_size;
    unsigned int next_offset;
    /* Session cookie */
    char         session_cookie[JK_SHM_STR_SIZ+1];
    /* Session path */
    char         session_path[JK_SHM_STR_SIZ+1];
};
typedef struct lb_worker lb_worker_t;

int JK_METHOD lb_worker_factory(jk_worker_t **w,
                                const char *name, jk_logger_t *l);

const char *jk_lb_get_lock(lb_worker_t *p, jk_logger_t *l);
int jk_lb_get_lock_code(const char *v);
const char *jk_lb_get_method(lb_worker_t *p, jk_logger_t *l);
int jk_lb_get_method_code(const char *v);
const char *jk_lb_get_state(lb_sub_worker_t *p, jk_logger_t *l);
int jk_lb_get_state_code(const char *v);
const char *jk_lb_get_activation_direct(int activation, jk_logger_t *l);
const char *jk_lb_get_activation(lb_sub_worker_t *p, jk_logger_t *l);
int jk_lb_get_activation_code(const char *v);
void reset_lb_values(lb_worker_t *p, jk_logger_t *l);
void jk_lb_pull(lb_worker_t * p, int locked, jk_logger_t *l);
void jk_lb_push(lb_worker_t * p, int locked, jk_logger_t *l);
void update_mult(lb_worker_t * p, jk_logger_t *l);

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* JK_LB_WORKER_H */
