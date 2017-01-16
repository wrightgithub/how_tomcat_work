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
 * Description: Workers controller                                         *
 * Author:      Gal Shachor <shachor@il.ibm.com>                           *
 * Author:      Henri Gomez <hgomez@apache.org>                            *
 * Version:     $Revision: 708019 $                                          *
 ***************************************************************************/

#define _PLACE_WORKER_LIST_HERE
#include "jk_worker_list.h"
#include "jk_worker.h"
#include "jk_util.h"
#include "jk_mt.h"

static void close_workers(jk_logger_t *l);

static worker_factory get_factory_for(const char *type);

static int build_worker_map(jk_map_t *init_data,
                            char **worker_list,
                            unsigned num_of_workers,
                            jk_worker_env_t *we, jk_logger_t *l);

/* Global worker list */
static jk_map_t *worker_map;
#if _MT_CODE
static JK_CRIT_SEC worker_lock;
#endif
static int worker_maintain_time = 0;

int wc_open(jk_map_t *init_data, jk_worker_env_t *we, jk_logger_t *l)
{
    int rc;
    JK_TRACE_ENTER(l);

    if (!jk_map_alloc(&worker_map)) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    JK_INIT_CS(&worker_lock, rc);
    if (rc == JK_FALSE) {
        jk_log(l, JK_LOG_ERROR,
                "creating thread lock (errno=%d)",
                errno);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    jk_map_dump(init_data, l);
    we->init_data = init_data;
    if (!jk_get_worker_list(init_data, &(we->worker_list),
                            &we->num_of_workers)) {
        JK_TRACE_EXIT(l);
        we->num_of_workers = 0;
        we->worker_list = NULL;
        return JK_FALSE;
    }

    worker_maintain_time = jk_get_worker_maintain_time(init_data);
    if(worker_maintain_time < 0)
        worker_maintain_time = 0;

    if (!build_worker_map(init_data, we->worker_list,
                          we->num_of_workers, we, l)) {
        close_workers(l);
        we->num_of_workers = 0;
        we->worker_list = NULL;
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}


void wc_close(jk_logger_t *l)
{
    int rc;
    JK_TRACE_ENTER(l);
    JK_DELETE_CS(&worker_lock, rc);
    close_workers(l);
    JK_TRACE_EXIT(l);
}

jk_worker_t *wc_get_worker_for_name(const char *name, jk_logger_t *l)
{
    jk_worker_t *rc;

    JK_TRACE_ENTER(l);
    if (!name) {
        JK_LOG_NULL_PARAMS(l);
        JK_TRACE_EXIT(l);
        return NULL;
    }

    rc = jk_map_get(worker_map, name, NULL);

    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG, "%s a worker %s",
               rc ? "found" : "did not find", name);
    JK_TRACE_EXIT(l);
    return rc;
}

int wc_create_worker(const char *name, int use_map,
                     jk_map_t *init_data,
                     jk_worker_t **rc, jk_worker_env_t *we, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (rc) {
        const char *type = jk_get_worker_type(init_data, name);
        worker_factory fac = get_factory_for(type);
        jk_worker_t *w = NULL;
        unsigned int i, num_of_maps;
        char **map_names;
        int wtype;

        *rc = NULL;

        if (!fac) {
            jk_log(l, JK_LOG_ERROR, "Unknown worker type %s for worker %s",
                   type, name);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "about to create instance %s of %s", name,
                   type);

        if (((wtype = fac(&w, name, l)) == 0) || !w) {
            jk_log(l, JK_LOG_ERROR,
                   "factory for %s failed for %s", type,
                   name);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "about to validate and init %s", name);
        if (!w->validate(w, init_data, we, l)) {
            w->destroy(&w, l);
            jk_log(l, JK_LOG_ERROR,
                   "validate failed for %s", name);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        if (!w->init(w, init_data, we, l)) {
            w->destroy(&w, l);
            jk_log(l, JK_LOG_ERROR, "init failed for %s",
                   name);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        if (use_map &&
            jk_get_worker_mount_list(init_data, name,
                                     &map_names,
                                     &num_of_maps) && num_of_maps) {
            for (i = 0; i < num_of_maps; i++) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                            "mounting %s to worker %s",
                            map_names[i], name);
                if (uri_worker_map_add(we->uri_to_worker, map_names[i],
                                       name, SOURCE_TYPE_WORKERDEF, l) == JK_FALSE) {
                    w->destroy(&w, l);
                    jk_log(l, JK_LOG_ERROR,
                           "mounting %s failed for %s",
                           map_names[i], name);
                    JK_TRACE_EXIT(l);
                    return JK_FALSE;
                }
            }
        }
        w->type = wtype;
        *rc = w;
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    return JK_FALSE;
}

static void close_workers(jk_logger_t *l)
{
    int sz = jk_map_size(worker_map);

    JK_TRACE_ENTER(l);

    if (sz > 0) {
        int i;
        for (i = 0; i < sz; i++) {
            jk_worker_t *w = jk_map_value_at(worker_map, i);
            if (w) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "close_workers will destroy worker %s",
                           jk_map_name_at(worker_map, i));
                w->destroy(&w, l);
            }
        }
    }
    jk_map_free(&worker_map);
    JK_TRACE_EXIT(l);
}

static int build_worker_map(jk_map_t *init_data,
                            char **worker_list,
                            unsigned num_of_workers,
                            jk_worker_env_t *we, jk_logger_t *l)
{
    unsigned i;

    JK_TRACE_ENTER(l);

    for (i = 0; i < num_of_workers; i++) {
        jk_worker_t *w = NULL;

        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "creating worker %s", worker_list[i]);

        if (wc_create_worker(worker_list[i], 1, init_data, &w, we, l)) {
            jk_worker_t *oldw = NULL;
            if (!jk_map_put(worker_map, worker_list[i], w, (void *)&oldw)) {
                w->destroy(&w, l);
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }

            if (oldw) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "removing old %s worker",
                           worker_list[i]);
                oldw->destroy(&oldw, l);
            }
        }
        else {
            jk_log(l, JK_LOG_ERROR,
                   "failed to create worker %s",
                   worker_list[i]);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static worker_factory get_factory_for(const char *type)
{
    worker_factory_record_t *factory = &worker_factories[0];
    while (factory->name) {
        if (0 == strcmp(factory->name, type)) {
            return factory->fac;
        }

        factory++;
    }

    return NULL;
}

const char *wc_get_name_for_type(int type, jk_logger_t *l)
{
    worker_factory_record_t *factory = &worker_factories[0];
    while (factory->name) {
        if (type == factory->type) {
            jk_log(l, JK_LOG_DEBUG,
                   "Found worker type '%s'",
                   factory->name);
            return factory->name;
        }

        factory++;
    }

    return NULL;
}

void wc_maintain(jk_logger_t *l)
{
    static time_t last_maintain = 0;
    static int    running_maintain = 0;
    int sz = jk_map_size(worker_map);

    JK_TRACE_ENTER(l);

    /* Only proceed if all of the below hold true:
     * - there are workers
     * - maintenance wasn't disabled by configuration
     * - time since last maintenance is big enough
     */
    if (sz > 0 && worker_maintain_time > 0 &&
        difftime(time(NULL), last_maintain) >= worker_maintain_time) {
        int i;
        JK_ENTER_CS(&worker_lock, i);
        if (running_maintain ||
            difftime(time(NULL), last_maintain) < worker_maintain_time) {
            /* Already in maintain */
            JK_LEAVE_CS(&worker_lock, i);
            JK_TRACE_EXIT(l);
            return;
        }
        /* Set the maintain run flag so other threads skip
         * the maintain until we are finished.
         */
        running_maintain = 1;
        JK_LEAVE_CS(&worker_lock, i);

        for (i = 0; i < sz; i++) {
            jk_worker_t *w = jk_map_value_at(worker_map, i);
            if (w && w->maintain) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Maintaining worker %s",
                           jk_map_name_at(worker_map, i));
                w->maintain(w, time(NULL), l);
            }
        }
        JK_ENTER_CS(&worker_lock, i);
        last_maintain = time(NULL);
        running_maintain = 0;
        JK_LEAVE_CS(&worker_lock, i);
    }
    JK_TRACE_EXIT(l);
}
