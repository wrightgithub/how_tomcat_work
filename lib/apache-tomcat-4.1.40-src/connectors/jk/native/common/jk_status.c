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
 * Description: Status worker, display and manages JK workers              *
 * Author:      Mladen Turk <mturk@jboss.com>                              *
 * Author:      Rainer Jung <rjung@apache.org>                             *
 * Version:     $Revision: 757040 $                                        *
 ***************************************************************************/

#include "jk_pool.h"
#include "jk_service.h"
#include "jk_util.h"
#include "jk_worker.h"
#include "jk_status.h"
#include "jk_mt.h"
#include "jk_shm.h"
#include "jk_ajp_common.h"
#include "jk_lb_worker.h"
#include "jk_ajp13_worker.h"
#include "jk_ajp14_worker.h"
#include "jk_connect.h"
#include "jk_uri_worker_map.h"

#define HUGE_BUFFER_SIZE (8*1024)

/**
 * Command line reference:
 * cmd=list (default) display configuration
 * cmd=show display detailed configuration
 * cmd=edit form to change configuration
 * cmd=update commit update configuration
 * cmd=reset reset lb runtime states, or lb member runtime states
 * cmd=version show only software version
 * Query arguments:
 * re=n (refresh time in seconds, n=0: disabled)
 * w=worker (cmd should be executed for worker "worker")
 * sw=sub_worker (cmd should be executed for "sub_worker" of worker "worker")
 * from=lastcmd (the last viewing command was "lastcmd")
 * opt=option (changes meaning of edit and list/show)
 */

#define JK_STATUS_ARG_CMD                  "cmd"
#define JK_STATUS_ARG_MIME                 "mime"
#define JK_STATUS_ARG_FROM                 "from"
#define JK_STATUS_ARG_REFRESH              "re"
#define JK_STATUS_ARG_WORKER               "w"
#define JK_STATUS_ARG_SUB_WORKER           "sw"
#define JK_STATUS_ARG_PREV_SUB_WORKER      "psw"
#define JK_STATUS_ARG_ATTRIBUTE            "att"
#define JK_STATUS_ARG_MULT_VALUE_BASE      "val"
#define JK_STATUS_ARG_OPTIONS              "opt"

#define JK_STATUS_ARG_OPTION_NO_MEMBERS    0x0001
#define JK_STATUS_ARG_OPTION_NO_MAPS       0x0002
#define JK_STATUS_ARG_OPTION_NO_LEGEND     0x0004
#define JK_STATUS_ARG_OPTION_NO_LB         0x0008
#define JK_STATUS_ARG_OPTION_NO_AJP        0x0010
#define JK_STATUS_ARG_OPTION_READ_ONLY     0x0020
#define JK_STATUS_ARG_OPTION_NO_LB_CONF    0x0040
#define JK_STATUS_ARG_OPTION_NO_LB_SUMMARY 0x0080
#define JK_STATUS_ARG_OPTION_NO_AJP_CONF   0x0100

#define JK_STATUS_ARG_LB_RETRIES           ("vlr")
#define JK_STATUS_ARG_LB_RETRY_INT         ("vlri")
#define JK_STATUS_ARG_LB_RECOVER_TIME      ("vlt")
#define JK_STATUS_ARG_LB_ERROR_ESCALATION_TIME ("vlee")
#define JK_STATUS_ARG_LB_MAX_REPLY_TIMEOUTS ("vlx")
#define JK_STATUS_ARG_LB_STICKY            ("vls")
#define JK_STATUS_ARG_LB_STICKY_FORCE      ("vlf")
#define JK_STATUS_ARG_LB_METHOD            ("vlm")
#define JK_STATUS_ARG_LB_LOCK              ("vll")

#define JK_STATUS_ARG_LB_TEXT_RETRIES      "Retries"
#define JK_STATUS_ARG_LB_TEXT_RETRY_INT    "Retry Interval"
#define JK_STATUS_ARG_LB_TEXT_RECOVER_TIME "Recover Wait Time"
#define JK_STATUS_ARG_LB_TEXT_ERROR_ESCALATION_TIME "Error Escalation Time"
#define JK_STATUS_ARG_LB_TEXT_MAX_REPLY_TIMEOUTS "Max Reply Timeouts"
#define JK_STATUS_ARG_LB_TEXT_STICKY       "Sticky Sessions"
#define JK_STATUS_ARG_LB_TEXT_STICKY_FORCE "Force Sticky Sessions"
#define JK_STATUS_ARG_LB_TEXT_METHOD       "LB Method"
#define JK_STATUS_ARG_LB_TEXT_LOCK         "Locking"

#define JK_STATUS_ARG_LBM_ACTIVATION       ("vwa")
#define JK_STATUS_ARG_LBM_FACTOR           ("vwf")
#define JK_STATUS_ARG_LBM_ROUTE            ("vwn")
#define JK_STATUS_ARG_LBM_REDIRECT         ("vwr")
#define JK_STATUS_ARG_LBM_DOMAIN           ("vwc")
#define JK_STATUS_ARG_LBM_DISTANCE         ("vwd")

#define JK_STATUS_ARG_LBM_TEXT_ACTIVATION  "Activation"
#define JK_STATUS_ARG_LBM_TEXT_FACTOR      "LB Factor"
#define JK_STATUS_ARG_LBM_TEXT_ROUTE       "Route"
#define JK_STATUS_ARG_LBM_TEXT_REDIRECT    "Redirect Route"
#define JK_STATUS_ARG_LBM_TEXT_DOMAIN      "Cluster Domain"
#define JK_STATUS_ARG_LBM_TEXT_DISTANCE    "Distance"

#define JK_STATUS_ARG_AJP_CACHE_TO         "vacpt"
#define JK_STATUS_ARG_AJP_PING_TO          "vapng"
#define JK_STATUS_ARG_AJP_CONNECT_TO       "vact"
#define JK_STATUS_ARG_AJP_PREPOST_TO       "vapt"
#define JK_STATUS_ARG_AJP_REPLY_TO         "vart"
#define JK_STATUS_ARG_AJP_RETRIES          "var"
#define JK_STATUS_ARG_AJP_RETRY_INT        "vari"
#define JK_STATUS_ARG_AJP_REC_OPTS         "varo"
#define JK_STATUS_ARG_AJP_MAX_PK_SZ        "vamps"
#define JK_STATUS_ARG_AJP_CPING_INT        "vacpi"
#define JK_STATUS_ARG_AJP_HOST_STR         "vahst"
#define JK_STATUS_ARG_AJP_PORT             "vaprt"

#define JK_STATUS_ARG_AJP_TEXT_CACHE_TO    "Connection Pool Timeout"
#define JK_STATUS_ARG_AJP_TEXT_PING_TO     "Ping Timeout"
#define JK_STATUS_ARG_AJP_TEXT_CONNECT_TO  "Connect Timeout"
#define JK_STATUS_ARG_AJP_TEXT_PREPOST_TO  "Prepost Timeout"
#define JK_STATUS_ARG_AJP_TEXT_REPLY_TO    "Reply Timeout"
#define JK_STATUS_ARG_AJP_TEXT_RETRIES     "Retries"
#define JK_STATUS_ARG_AJP_TEXT_RETRY_INT   "Retry Interval"
#define JK_STATUS_ARG_AJP_TEXT_REC_OPTS    "Recovery Options"
#define JK_STATUS_ARG_AJP_TEXT_MAX_PK_SZ   "Max Packet Size"
#define JK_STATUS_ARG_AJP_TEXT_CPING_INT   "Connection Ping Interval"
#define JK_STATUS_ARG_AJP_TEXT_HOST_STR    "Hostname"
#define JK_STATUS_ARG_AJP_TEXT_PORT        "Port"
#define JK_STATUS_ARG_AJP_TEXT_ADDR_STR    "Address:Port"

#define JK_STATUS_CMD_UNKNOWN              (0)
#define JK_STATUS_CMD_LIST                 (1)
#define JK_STATUS_CMD_SHOW                 (2)
#define JK_STATUS_CMD_EDIT                 (3)
#define JK_STATUS_CMD_UPDATE               (4)
#define JK_STATUS_CMD_RESET                (5)
#define JK_STATUS_CMD_VERSION              (6)
#define JK_STATUS_CMD_RECOVER              (7)
#define JK_STATUS_CMD_DUMP                 (8)
#define JK_STATUS_CMD_DEF                  (JK_STATUS_CMD_LIST)
#define JK_STATUS_CMD_MAX                  (JK_STATUS_CMD_DUMP)
#define JK_STATUS_CMD_TEXT_UNKNOWN         ("unknown")
#define JK_STATUS_CMD_TEXT_LIST            ("list")
#define JK_STATUS_CMD_TEXT_SHOW            ("show")
#define JK_STATUS_CMD_TEXT_EDIT            ("edit")
#define JK_STATUS_CMD_TEXT_UPDATE          ("update")
#define JK_STATUS_CMD_TEXT_RESET           ("reset")
#define JK_STATUS_CMD_TEXT_VERSION         ("version")
#define JK_STATUS_CMD_TEXT_RECOVER         ("recover")
#define JK_STATUS_CMD_TEXT_DUMP            ("dump")
#define JK_STATUS_CMD_TEXT_DEF             (JK_STATUS_CMD_TEXT_LIST)

#define JK_STATUS_CMD_PROP_CHECK_WORKER    0x00000001
#define JK_STATUS_CMD_PROP_READONLY        0x00000002
#define JK_STATUS_CMD_PROP_HEAD            0x00000004
#define JK_STATUS_CMD_PROP_REFRESH         0x00000008
#define JK_STATUS_CMD_PROP_BACK_LINK       0x00000010
#define JK_STATUS_CMD_PROP_BACK_LIST       0x00000020
#define JK_STATUS_CMD_PROP_FMT             0x00000040
#define JK_STATUS_CMD_PROP_SWITCH_RO       0x00000080
#define JK_STATUS_CMD_PROP_DUMP_LINK       0x00000100
#define JK_STATUS_CMD_PROP_LINK_HELP       0x00000200
#define JK_STATUS_CMD_PROP_LEGEND          0x00000400
#define JK_STATUS_CMD_PROP_WILDCARD        0x00000800

#define JK_STATUS_MIME_UNKNOWN             (0)
#define JK_STATUS_MIME_HTML                (1)
#define JK_STATUS_MIME_XML                 (2)
#define JK_STATUS_MIME_TXT                 (3)
#define JK_STATUS_MIME_PROP                (4)
#define JK_STATUS_MIME_DEF                 (JK_STATUS_MIME_HTML)
#define JK_STATUS_MIME_MAX                 (JK_STATUS_MIME_PROP)
#define JK_STATUS_MIME_TEXT_UNKNOWN        ("unknown")
#define JK_STATUS_MIME_TEXT_HTML           ("html")
#define JK_STATUS_MIME_TEXT_XML            ("xml")
#define JK_STATUS_MIME_TEXT_TXT            ("txt")
#define JK_STATUS_MIME_TEXT_PROP           ("prop")
#define JK_STATUS_MIME_TEXT_DEF            (JK_STATUS_MIME_TEXT_HTML)

#define JK_STATUS_MASK_ACTIVE              0x000000FF
#define JK_STATUS_MASK_DISABLED            0x0000FF00
#define JK_STATUS_MASK_STOPPED             0x00FF0000
#define JK_STATUS_MASK_OK                  0x00010101
#define JK_STATUS_MASK_IDLE                0x00020202
#define JK_STATUS_MASK_BUSY                0x00040404
#define JK_STATUS_MASK_RECOVER             0x00080808
#define JK_STATUS_MASK_ERROR               0x00101010
#define JK_STATUS_MASK_GOOD_DEF            0x0000000F
#define JK_STATUS_MASK_BAD_DEF             0x00FF1010

#define JK_STATUS_NEEDS_PUSH               0x00000001
#define JK_STATUS_NEEDS_RESET_LB_VALUES    0x00000002
#define JK_STATUS_NEEDS_UPDATE_MULT        0x00000004
#define JK_STATUS_NEEDS_ADDR_PUSH          0x00000008

#define JK_STATUS_WAIT_AFTER_UPDATE        "3"
#define JK_STATUS_REFRESH_DEF              "10"
#define JK_STATUS_ESC_CHARS                ("<>?\"")
#define JK_STATUS_TIME_FMT_HTML            "%a, %d %b %Y %X %Z"
#define JK_STATUS_TIME_FMT_TEXT            "%Y%m%d%H%M%S"
#define JK_STATUS_TIME_FMT_TZ              "%Z"
#define JK_STATUS_TIME_BUF_SZ              (80)

#define JK_STATUS_HEAD                     "<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?>\n" \
                                           "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\"" \
                                           " \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n" \
                                           "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">" \
                                           "<head><title>JK Status Manager</title>"

#define JK_STATUS_COPYRIGHT                "Copyright &#169; 1999-2009, The Apache Software Foundation<br />" \
                                           "Licensed under the <a href=\"http://www.apache.org/licenses/LICENSE-2.0\">" \
                                           "Apache License, Version 2.0</a>."

#define JK_STATUS_HEND                     "</head>\n<body>\n"
#define JK_STATUS_BEND                     "</body>\n</html>\n"

#define JK_STATUS_XMLH                     "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"

#define JK_STATUS_NS_DEF                   "jk:"
#define JK_STATUS_XMLNS_DEF                "xmlns:jk=\"http://tomcat.apache.org\""
#define JK_STATUS_PREFIX_DEF               "worker"

#define JK_STATUS_FORM_START               "<form method=\"%s\" action=\"%s\">\n"
#define JK_STATUS_FORM_HIDDEN_INT          "<input type=\"hidden\" name=\"%s\" value=\"%d\"/>\n"
#define JK_STATUS_FORM_HIDDEN_STRING       "<input type=\"hidden\" name=\"%s\" value=\"%s\"/>\n"
#define JK_STATUS_URI_MAP_TABLE_HEAD       "<tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th></tr>\n"
#define JK_STATUS_URI_MAP_TABLE_ROW        "<tr><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td></tr>\n"
#define JK_STATUS_URI_MAP_TABLE_HEAD2      "<tr><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th><th>%s</th></tr>\n"
#define JK_STATUS_URI_MAP_TABLE_ROW2       "<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%d</td></tr>\n"
#define JK_STATUS_SHOW_AJP_CONF_HEAD       "<tr>" \
                                           "<th>Type</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_HOST_STR "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_ADDR_STR "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_CACHE_TO "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_CONNECT_TO "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_PREPOST_TO "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_REPLY_TO "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_RETRIES "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_REC_OPTS "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_MAX_PK_SZ "</th>" \
                                           "<th>\n"
#define JK_STATUS_SHOW_AJP_CONF_ROW        "<tr>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%u</td>" \
                                           "<td>%u</td>" \
                                           "<td></td>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_AJP_HEAD            "<tr>" \
                                           "<th>State</th>" \
                                           "<th>Acc</th>" \
                                           "<th>Err</th><th>CE</th><th>RE</th>" \
                                           "<th>Wr</th><th>Rd</th><th>Busy</th><th>Max</th><th>LR</th><th>LE</th>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_AJP_ROW             "<tr>" \
                                           "<td>%s</td>" \
                                           "<td>%" JK_UINT64_T_FMT " (%d/sec)</td>" \
                                           "<td>%" JK_UINT32_T_FMT "</td>" \
                                           "<td>%" JK_UINT32_T_FMT "</td>" \
                                           "<td>%" JK_UINT32_T_FMT "</td>" \
                                           "<td>%s (%s/sec)</td>" \
                                           "<td>%s (%s/sec)</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%s</td>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_LB_HEAD             "<tr>" \
                                           "<th>Type</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_STICKY "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_STICKY_FORCE "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_RETRIES "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_METHOD "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_LOCK "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_RECOVER_TIME "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_ERROR_ESCALATION_TIME "</th>" \
                                           "<th>" JK_STATUS_ARG_LB_TEXT_MAX_REPLY_TIMEOUTS "</th>" \
                                           "<th>\n"
#define JK_STATUS_SHOW_LB_ROW              "<tr>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td></td>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_MEMBER_HEAD         "<tr>" \
                                           "<th>&nbsp;</th><th>Name</th>" \
                                           "<th>Act</th><th>State</th>" \
                                           "<th>D</th><th>F</th><th>M</th>" \
                                           "<th>V</th><th>Acc</th>" \
                                           "<th>Err</th><th>CE</th><th>RE</th>" \
                                           "<th>Wr</th><th>Rd</th><th>Busy</th><th>Max</th>" \
                                           "<th>" JK_STATUS_ARG_LBM_TEXT_ROUTE "</th>" \
                                           "<th>RR</th><th>Cd</th><th>Rs</th><th>LR</th><th>LE</th>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_MEMBER_ROW          "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%" JK_UINT64_T_FMT "</td>" \
                                           "<td>%" JK_UINT64_T_FMT "</td>" \
                                           "<td>%" JK_UINT64_T_FMT " (%d/sec)</td>" \
                                           "<td>%" JK_UINT32_T_FMT "</td>" \
                                           "<td>%" JK_UINT32_T_FMT "</td>" \
                                           "<td>%" JK_UINT32_T_FMT "</td>" \
                                           "<td>%s (%s/sec)</td>" \
                                           "<td>%s (%s/sec)</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d/%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%s</td>" \
                                           "</tr>\n"
#define JK_STATUS_SHOW_MEMBER_CONF_HEAD    "<tr>" \
                                           "<th>Name</th><th>Type</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_HOST_STR "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_ADDR_STR "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_CACHE_TO "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_CONNECT_TO "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_PREPOST_TO "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_REPLY_TO "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_RETRIES "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_REC_OPTS "</th>" \
                                           "<th>" JK_STATUS_ARG_AJP_TEXT_MAX_PK_SZ "</th>" \
                                           "<th>\n"
#define JK_STATUS_SHOW_MEMBER_CONF_ROW     "<tr>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%s</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%d</td>" \
                                           "<td>%u</td>" \
                                           "<td>%u</td>" \
                                           "<td></td>" \
                                           "</tr>\n"

typedef struct status_worker status_worker_t;

struct status_endpoint
{
    status_worker_t *worker;

    char            *query_string;
    jk_map_t        *req_params;
    char            *msg;

    jk_endpoint_t   endpoint;

};

typedef struct status_endpoint status_endpoint_t;

struct status_worker
{
    jk_pool_t         p;
    jk_pool_atom_t    buf[TINY_POOL_SIZE];
    const char        *name;
    const char        *css;
    const char        *ns;
    const char        *xmlns;
    const char        *doctype;
    const char        *prefix;
    int               read_only;
    char              **user_names;
    unsigned int      num_of_users;
    int               user_case_insensitive;
    jk_uint32_t       good_mask;
    jk_uint32_t       bad_mask;
    jk_worker_t       worker;
    jk_worker_env_t   *we;
};

static const char *worker_type[] = {
    "unknown",
    "ajp12",
    "ajp13",
    "ajp14",
    "jni",
    "lb",
    "status",
    NULL
};

static const char *headers_names[] = {
    "Content-Type",
    "Cache-Control",
    "Pragma",
    NULL
};

static const char *cmd_type[] = {
    JK_STATUS_CMD_TEXT_UNKNOWN,
    JK_STATUS_CMD_TEXT_LIST,
    JK_STATUS_CMD_TEXT_SHOW,
    JK_STATUS_CMD_TEXT_EDIT,
    JK_STATUS_CMD_TEXT_UPDATE,
    JK_STATUS_CMD_TEXT_RESET,
    JK_STATUS_CMD_TEXT_VERSION,
    JK_STATUS_CMD_TEXT_RECOVER,
    JK_STATUS_CMD_TEXT_DUMP,
    NULL
};

static const char *mime_type[] = {
    JK_STATUS_MIME_TEXT_UNKNOWN,
    JK_STATUS_MIME_TEXT_HTML,
    JK_STATUS_MIME_TEXT_XML,
    JK_STATUS_MIME_TEXT_TXT,
    JK_STATUS_MIME_TEXT_PROP,
    NULL
};

#define HEADERS_NO_CACHE "no-cache", "no-cache", NULL

static const char *headers_vhtml[] = {
    "text/html",
    HEADERS_NO_CACHE
};

static const char *headers_vxml[] = {
    "text/xml",
    HEADERS_NO_CACHE
};

static const char *headers_vtxt[] = {
    "text/plain",
    HEADERS_NO_CACHE
};

static void jk_puts(jk_ws_service_t *s, const char *str)
{
    if (str)
        s->write(s, str, (unsigned int)strlen(str));
    else
        s->write(s, "(null)", 6);
}

static void jk_putv(jk_ws_service_t *s, ...)
{
    va_list va;
    const char *str;

    va_start(va, s);
    while (1) {
        str = va_arg(va, const char *);
        if (str == NULL)
            break;
        s->write(s, str, (unsigned int)strlen(str));
    }
    va_end(va);
}

static int jk_printf(jk_ws_service_t *s, const char *fmt, ...)
{
    int rc = 0;
    va_list args;
#ifdef NETWARE
/* On NetWare, this can get called on a thread that has a limited stack so */
/* we will allocate and free the temporary buffer in this function         */
        char *buf;
#else
        char buf[HUGE_BUFFER_SIZE];
#endif

    if (!s || !fmt) {
        return -1;
    }
    va_start(args, fmt);

#ifdef NETWARE
        buf = (char *)malloc(HUGE_BUFFER_SIZE);
        if (NULL == buf)
            return -1;
#endif
    rc = vsnprintf(buf, HUGE_BUFFER_SIZE, fmt, args);
    va_end(args);
    if (rc > 0)
        s->write(s, buf, rc);
#ifdef NETWARE
        free(buf);
#endif
    return rc;
}

static void jk_print_xml_start_elt(jk_ws_service_t *s, status_worker_t *w,
                                   int indentation, int close_tag,
                                   const char *name)
{
    if (close_tag) {
        jk_printf(s, "%*s<%s%s>\n", indentation, "", w->ns, name);
    }
    else {
        jk_printf(s, "%*s<%s%s\n", indentation, "", w->ns, name);
    }
}

static void jk_print_xml_close_elt(jk_ws_service_t *s, status_worker_t *w,
                                   int indentation,
                                   const char *name)
{
    jk_printf(s, "%*s</%s%s>\n", indentation, "", w->ns, name);
}

static void jk_print_xml_stop_elt(jk_ws_service_t *s,
                                  int indentation, int close_tag)
{
    if (close_tag) {
        jk_printf(s, "%*s/>\n", indentation, "");
    }
    else {
        jk_printf(s, "%*s>\n", indentation, "");
    }
}

static void jk_print_xml_att_string(jk_ws_service_t *s,
                                    int indentation,
                                    const char *key, const char *value)
{
    jk_printf(s, "%*s%s=\"%s\"\n", indentation, "", key, value ? value : "");
}

static void jk_print_xml_att_int(jk_ws_service_t *s,
                                 int indentation,
                                 const char *key, int value)
{
    jk_printf(s, "%*s%s=\"%d\"\n", indentation, "", key, value);
}

static void jk_print_xml_att_uint(jk_ws_service_t *s,
                                  int indentation,
                                  const char *key, unsigned int value)
{
    jk_printf(s, "%*s%s=\"%u\"\n", indentation, "", key, value);
}

static void jk_print_xml_att_long(jk_ws_service_t *s,
                                  int indentation,
                                  const char *key, long value)
{
    jk_printf(s, "%*s%s=\"%ld\"\n", indentation, "", key, value);
}

static void jk_print_xml_att_uint32(jk_ws_service_t *s,
                                    int indentation,
                                    const char *key, jk_uint32_t value)
{
    jk_printf(s, "%*s%s=\"%" JK_UINT32_T_FMT "\"\n", indentation, "", key, value);
}

static void jk_print_xml_att_uint64(jk_ws_service_t *s,
                                    int indentation,
                                    const char *key, jk_uint64_t value)
{
    jk_printf(s, "%*s%s=\"%" JK_UINT64_T_FMT "\"\n", indentation, "", key, value);
}

static void jk_print_prop_att_string(jk_ws_service_t *s, status_worker_t *w,
                                     const char *name,
                                     const char *key, const char *value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%s\n", w->prefix, name, key, value ? value : "");
    }
    else {
        jk_printf(s, "%s.%s=%s\n", w->prefix, key, value ? value : "");
    }
}

static void jk_print_prop_att_int(jk_ws_service_t *s, status_worker_t *w,
                                  const char *name,
                                  const char *key, int value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%d\n", w->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%d\n", w->prefix, key, value);
    }
}

static void jk_print_prop_att_uint(jk_ws_service_t *s, status_worker_t *w,
                                   const char *name,
                                   const char *key, unsigned int value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%u\n", w->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%u\n", w->prefix, key, value);
    }
}

static void jk_print_prop_att_long(jk_ws_service_t *s, status_worker_t *w,
                                   const char *name,
                                   const char *key, long value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%ld\n", w->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%ld\n", w->prefix, key, value);
    }
}

static void jk_print_prop_att_uint32(jk_ws_service_t *s, status_worker_t *w,
                                     const char *name,
                                     const char *key, jk_uint32_t value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%" JK_UINT32_T_FMT "\n", w->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%" JK_UINT32_T_FMT "\n", w->prefix, key, value);
    }
}

static void jk_print_prop_att_uint64(jk_ws_service_t *s, status_worker_t *w,
                                     const char *name,
                                     const char *key, jk_uint64_t value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s=%" JK_UINT64_T_FMT "\n", w->prefix, name, key, value);
    }
    else {
        jk_printf(s, "%s.%s=%" JK_UINT64_T_FMT "\n", w->prefix, key, value);
    }
}

static void jk_print_prop_item_int(jk_ws_service_t *s, status_worker_t *w,
                                   const char *name, const char *list, int num,
                                   const char *key, int value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s.%d.%s=%d\n", w->prefix, name, list, num, key, value);
    }
    else {
        jk_printf(s, "%s.%s.%d.%s=%d\n", w->prefix, list, num, key, value);
    }
}

static void jk_print_prop_item_string(jk_ws_service_t *s, status_worker_t *w,
                                      const char *name, const char *list, int num,
                                      const char *key, const char *value)
{
    if (name) {
        jk_printf(s, "%s.%s.%s.%d.%s=%s\n", w->prefix, name, list, num, key, value ? value : "");
    }
    else {
        jk_printf(s, "%s.%s.%d.%s=%s\n", w->prefix, list, num, key, value ? value : "");
    }
}

/* Actually APR's apr_strfsize */
static char *status_strfsize(jk_uint64_t size, char *buf)
{
    const char ord[] = "KMGTPE";
    const char *o = ord;
    unsigned int remain, siz;

    if (size < 973) {
        if (sprintf(buf, "%d ", (int) size) < 0)
            return strcpy(buf, "****");
        return buf;
    }
    do {
        remain = (unsigned int)(size & 0x03FF);
        size >>= 10;
        if (size >= 973) {
            ++o;
            continue;
        }
        siz = (unsigned int)(size & 0xFFFF);
        if (siz < 9 || (siz == 9 && remain < 973)) {
            if ((remain = ((remain * 5) + 256) / 512) >= 10)
                ++siz, remain = 0;
            if (sprintf(buf, "%d.%d%c", siz, remain, *o) < 0)
                return strcpy(buf, "****");
            return buf;
        }
        if (remain >= 512)
            ++siz;
        if (sprintf(buf, "%d%c", siz, *o) < 0)
            return strcpy(buf, "****");
        return buf;
    } while (1);
}

static int status_strftime(time_t clock, int mime, char *buf_time, char *buf_tz,
                           jk_logger_t *l)
{
    int rc_time;
#ifdef _MT_CODE_PTHREAD
    struct tm res;
    struct tm *tms = localtime_r(&clock, &res);
#else
    struct tm *tms = localtime(&clock);
#endif

    JK_TRACE_ENTER(l);

    if (mime == JK_STATUS_MIME_HTML)
        rc_time = strftime(buf_time, JK_STATUS_TIME_BUF_SZ, JK_STATUS_TIME_FMT_HTML, tms);
    else {
        rc_time = strftime(buf_time, JK_STATUS_TIME_BUF_SZ, JK_STATUS_TIME_FMT_TEXT, tms);
    }
    strftime(buf_tz, JK_STATUS_TIME_BUF_SZ, JK_STATUS_TIME_FMT_TZ, tms);

    JK_TRACE_EXIT(l);
    return rc_time;

}

static int status_rate(lb_sub_worker_t *wr, status_worker_t *w,
                       jk_logger_t *l)
{
    jk_uint32_t mask = 0;
    int activation = wr->activation;
    int state = wr->s->state;
    jk_uint32_t good = w->good_mask;
    jk_uint32_t bad = w->bad_mask;
    int rv = 0;

    switch (activation)
    {
    case JK_LB_ACTIVATION_ACTIVE:
        mask = JK_STATUS_MASK_ACTIVE;
        break;
    case JK_LB_ACTIVATION_DISABLED:
        mask = JK_STATUS_MASK_DISABLED;
        break;
    case JK_LB_ACTIVATION_STOPPED:
        mask = JK_STATUS_MASK_STOPPED;
        break;
    default:
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' unknown activation type '%d'",
               w->name, activation);
    }
    switch (state)
    {
    case JK_LB_STATE_IDLE:
        mask &= JK_STATUS_MASK_IDLE;
        break;
    case JK_LB_STATE_OK:
        mask &= JK_STATUS_MASK_OK;
        break;
    case JK_LB_STATE_RECOVER:
        mask &= JK_STATUS_MASK_RECOVER;
        break;
    case JK_LB_STATE_FORCE:
        mask &= JK_STATUS_MASK_RECOVER;
        break;
    case JK_LB_STATE_BUSY:
        mask &= JK_STATUS_MASK_BUSY;
        break;
    case JK_LB_STATE_ERROR:
        mask &= JK_STATUS_MASK_ERROR;
        break;
    case JK_LB_STATE_PROBE:
        mask &= JK_STATUS_MASK_RECOVER;
        break;
    default:
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' unknown state type '%d'",
               w->name, state);
    }
    if (mask&bad)
        rv = -1;
    else if (mask&good)
        rv = 1;
    else
        rv = 0;
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Status worker '%s' rating of activation '%s' and state '%s' for good '%08" JK_UINT32_T_HEX_FMT
               "' and bad '%08" JK_UINT32_T_HEX_FMT "' is %d",
               w->name, jk_lb_get_activation(wr, l), jk_lb_get_state(wr, l),
               good, bad, rv);
    return rv;
}

static jk_uint32_t status_get_single_rating(const char rating, jk_logger_t *l)
{
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "rating retrieval for '%c'",
               rating);
    switch (rating)
    {
    case 'A':
    case 'a':
        return JK_STATUS_MASK_ACTIVE;
    case 'D':
    case 'd':
        return JK_STATUS_MASK_DISABLED;
    case 'S':
    case 's':
        return JK_STATUS_MASK_STOPPED;
    case 'O':
    case 'o':
        return JK_STATUS_MASK_OK;
    case 'I':
    case 'i':
    case 'N':
    case 'n':
        return JK_STATUS_MASK_IDLE;
    case 'B':
    case 'b':
        return JK_STATUS_MASK_BUSY;
    case 'R':
    case 'r':
        return JK_STATUS_MASK_RECOVER;
    case 'E':
    case 'e':
        return JK_STATUS_MASK_ERROR;
    default:
        jk_log(l, JK_LOG_WARNING,
               "Unknown rating type '%c'",
               rating);
        return 0;
    }
}

static jk_uint32_t status_get_rating(const char *rating,
                                     jk_logger_t *l)
{
    int off = 0;
    jk_uint32_t mask = 0;

    while (rating[off] == ' ' || rating[off] == '\t' || rating[off] == '.') {
        off++;
    }
    mask = status_get_single_rating(rating[off], l);
    while (rating[off] != '\0' && rating[off] != '.') {
        off++;
    }
    if (rating[off] == '.') {
        off++;
    }
    if (rating[off] != '\0') {
        mask &= status_get_single_rating(rating[off], l);
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "rating for '%s' is '%08" JK_UINT32_T_HEX_FMT "'",
               rating, mask);
    return mask;
}

static const char *status_worker_type(int t)
{
    if (t < 0 || t > 6)
        t = 0;
    return worker_type[t];
}


static int status_get_string(status_endpoint_t *p,
                             const char *param,
                             const char *def,
                             const char **result,
                             jk_logger_t *l)
{
    int rv;

    *result = jk_map_get_string(p->req_params,
                                param, NULL);
    if (*result) {
        rv = JK_TRUE;
    }
    else {
        *result = def;
        rv = JK_FALSE;
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "retrieved string arg '%s' as '%s'%s",
               param, *result ? *result : "(null)",
               rv == JK_FALSE ? " (default)" : "");
    return rv;
}

static int status_get_int(status_endpoint_t *p,
                          const char *param,
                          int def,
                          jk_logger_t *l)
{
    const char *arg;
    int rv = def;

    if (status_get_string(p, param, NULL, &arg, l) == JK_TRUE) {
        rv = atoi(arg);
    }
    return rv;
}

static int status_get_bool(status_endpoint_t *p,
                           const char *param,
                           int def,
                           jk_logger_t *l)
{
    const char *arg;

    if (status_get_string(p, param, NULL, &arg, l) == JK_TRUE) {
        return jk_get_bool_code(arg, def);
    }
    return def;
}

static const char *status_cmd_text(int cmd)
{
    return cmd_type[cmd];
}

static int status_cmd_int(const char *cmd)
{
    if (!cmd)
        return JK_STATUS_CMD_DEF;
    if (!strcmp(cmd, JK_STATUS_CMD_TEXT_LIST))
        return JK_STATUS_CMD_LIST;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_SHOW))
        return JK_STATUS_CMD_SHOW;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_EDIT))
        return JK_STATUS_CMD_EDIT;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_UPDATE))
        return JK_STATUS_CMD_UPDATE;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_RESET))
        return JK_STATUS_CMD_RESET;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_VERSION))
        return JK_STATUS_CMD_VERSION;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_RECOVER))
        return JK_STATUS_CMD_RECOVER;
    else if (!strcmp(cmd, JK_STATUS_CMD_TEXT_DUMP))
        return JK_STATUS_CMD_DUMP;
    return JK_STATUS_CMD_UNKNOWN;
}

static const char *status_mime_text(int mime)
{
    return mime_type[mime];
}

static int status_mime_int(const char *mime)
{
    if (!mime)
        return JK_STATUS_MIME_DEF;
    if (!strcmp(mime, JK_STATUS_MIME_TEXT_HTML))
        return JK_STATUS_MIME_HTML;
    else if (!strcmp(mime, JK_STATUS_MIME_TEXT_XML))
        return JK_STATUS_MIME_XML;
    else if (!strcmp(mime, JK_STATUS_MIME_TEXT_TXT))
        return JK_STATUS_MIME_TXT;
    else if (!strcmp(mime, JK_STATUS_MIME_TEXT_PROP))
        return JK_STATUS_MIME_PROP;
    return JK_STATUS_MIME_UNKNOWN;
}

static jk_uint32_t status_cmd_props(int cmd)
{
    jk_uint32_t props = 0;

    if (cmd == JK_STATUS_CMD_LIST ||
        cmd == JK_STATUS_CMD_SHOW)
        props |= JK_STATUS_CMD_PROP_REFRESH |
                 JK_STATUS_CMD_PROP_SWITCH_RO |
                 JK_STATUS_CMD_PROP_LINK_HELP |
                 JK_STATUS_CMD_PROP_LEGEND;
    if (cmd == JK_STATUS_CMD_LIST ||
        cmd == JK_STATUS_CMD_SHOW ||
        cmd == JK_STATUS_CMD_VERSION)
        props |= JK_STATUS_CMD_PROP_DUMP_LINK;
    if (cmd == JK_STATUS_CMD_LIST ||
        cmd == JK_STATUS_CMD_SHOW ||
        cmd == JK_STATUS_CMD_VERSION ||
        cmd == JK_STATUS_CMD_DUMP)
        props |= JK_STATUS_CMD_PROP_HEAD |
                 JK_STATUS_CMD_PROP_FMT;
    if (cmd == JK_STATUS_CMD_SHOW ||
        cmd == JK_STATUS_CMD_VERSION ||
        cmd == JK_STATUS_CMD_DUMP)
        props |= JK_STATUS_CMD_PROP_BACK_LIST;
    if (cmd == JK_STATUS_CMD_SHOW ||
        cmd == JK_STATUS_CMD_EDIT ||
        cmd == JK_STATUS_CMD_VERSION ||
        cmd == JK_STATUS_CMD_DUMP)
        props |= JK_STATUS_CMD_PROP_BACK_LINK;
    if (cmd == JK_STATUS_CMD_UPDATE)
        props |= JK_STATUS_CMD_PROP_WILDCARD;
    if (cmd != JK_STATUS_CMD_EDIT &&
        cmd != JK_STATUS_CMD_UPDATE &&
        cmd != JK_STATUS_CMD_RESET &&
        cmd != JK_STATUS_CMD_RECOVER)
        props |= JK_STATUS_CMD_PROP_READONLY;
    if (cmd != JK_STATUS_CMD_LIST &&
        cmd != JK_STATUS_CMD_VERSION &&
        cmd != JK_STATUS_CMD_DUMP)
        props |= JK_STATUS_CMD_PROP_CHECK_WORKER;

    return props;
}

static void status_start_form(jk_ws_service_t *s,
                              status_endpoint_t *p,
                              const char *method,
                              int cmd,
                              const char *overwrite,
                              jk_logger_t *l)
{

    int i;
    int sz;
    jk_map_t *m = p->req_params;

    if (method)
        jk_printf(s, JK_STATUS_FORM_START, method, s->req_uri);
    else
        return;
    if (cmd != JK_STATUS_CMD_UNKNOWN) {
        jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING,
                  JK_STATUS_ARG_CMD, status_cmd_text(cmd));
    }

    sz = jk_map_size(m);
    for (i = 0; i < sz; i++) {
        const char *k = jk_map_name_at(m, i);
        const char *v = jk_map_value_at(m, i);
        if ((strcmp(k, JK_STATUS_ARG_CMD) ||
            cmd == JK_STATUS_CMD_UNKNOWN) &&
            (!overwrite ||
            strcmp(k, overwrite))) {
            jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING, k, v);
        }
    }
}

static void status_write_uri(jk_ws_service_t *s,
                             status_endpoint_t *p,
                             const char *text,
                             int cmd, int mime,
                             const char *worker, const char *sub_worker,
                             unsigned int add_options, unsigned int rm_options,
                             const char *attribute,
                             jk_logger_t *l)
{
    int i;
    int sz;
    int started = 0;
    int from;
    int restore_sub_worker = JK_FALSE;
    int save_sub_worker = JK_FALSE;
    int prev;
    unsigned int opt = 0;
    const char *arg;
    jk_map_t *m = p->req_params;

    if (text)
        jk_puts(s, "<a href=\"");
    jk_puts(s, s->req_uri);
    status_get_string(p, JK_STATUS_ARG_FROM, NULL, &arg, l);
    from = status_cmd_int(arg);
    status_get_string(p, JK_STATUS_ARG_CMD, NULL, &arg, l);
    prev = status_cmd_int(arg);
    if (cmd == JK_STATUS_CMD_SHOW && prev == JK_STATUS_CMD_EDIT) {
        restore_sub_worker = JK_TRUE;
    }
    if (cmd == JK_STATUS_CMD_UNKNOWN) {
        if (prev == JK_STATUS_CMD_UPDATE ||
            prev == JK_STATUS_CMD_RESET ||
            prev == JK_STATUS_CMD_RECOVER) {
            cmd = from;
            restore_sub_worker = JK_TRUE;
        }
    }
    if (cmd != JK_STATUS_CMD_UNKNOWN) {
        jk_printf(s, "%s%s=%s", started ? "&amp;" : "?",
                  JK_STATUS_ARG_CMD, status_cmd_text(cmd));
        if (cmd == JK_STATUS_CMD_EDIT ||
            cmd == JK_STATUS_CMD_RESET ||
            cmd == JK_STATUS_CMD_RECOVER) {
            jk_printf(s, "%s%s=%s", "&amp;",
                      JK_STATUS_ARG_FROM, status_cmd_text(prev));
            save_sub_worker = JK_TRUE;
        }
        started=1;
    }
    if (mime != JK_STATUS_MIME_UNKNOWN) {
        jk_printf(s, "%s%s=%s", started ? "&amp;" : "?",
                  JK_STATUS_ARG_MIME, status_mime_text(mime));
        started=1;
    }
    if (worker && worker[0]) {
        jk_printf(s, "%s%s=%s", started ? "&amp;" : "?",
                  JK_STATUS_ARG_WORKER, worker);
        started=1;
    }
    if (sub_worker && sub_worker[0] && cmd != JK_STATUS_CMD_LIST) {
        jk_printf(s, "%s%s=%s", started ? "&amp;" : "?",
                  JK_STATUS_ARG_SUB_WORKER, sub_worker);
        started=1;
    }
    if (attribute && attribute[0]) {
        jk_printf(s, "%s%s=%s", started ? "&amp;" : "?",
                  JK_STATUS_ARG_ATTRIBUTE, attribute);
        started=1;
    }

    sz = jk_map_size(m);
    for (i = 0; i < sz; i++) {
        const char *k = jk_map_name_at(m, i);
        const char *v = jk_map_value_at(m, i);
        if (!strcmp(k, JK_STATUS_ARG_CMD) && cmd != JK_STATUS_CMD_UNKNOWN) {
            continue;
        }
        if (!strcmp(k, JK_STATUS_ARG_MIME) && mime != JK_STATUS_MIME_UNKNOWN) {
            continue;
        }
        if (!strcmp(k, JK_STATUS_ARG_FROM)) {
            continue;
        }
        if (!strcmp(k, JK_STATUS_ARG_WORKER) && worker) {
            continue;
        }
        if (!strcmp(k, JK_STATUS_ARG_SUB_WORKER)) {
            if (save_sub_worker == JK_TRUE) {
                jk_printf(s, "%s%s=%s", started ? "&amp;" : "?",
                          JK_STATUS_ARG_PREV_SUB_WORKER, v);
                started=1;
                continue;
            }
            else if (sub_worker || cmd == JK_STATUS_CMD_LIST) {
                continue;
            }
            else if (restore_sub_worker == JK_TRUE) {
                continue;
            }
        }
        if (!strcmp(k, JK_STATUS_ARG_PREV_SUB_WORKER) && restore_sub_worker == JK_TRUE && cmd != JK_STATUS_CMD_LIST) {
            jk_printf(s, "%s%s=%s", started ? "&amp;" : "?",
                      JK_STATUS_ARG_SUB_WORKER, v);
            started=1;
            continue;
        }
        if (!strcmp(k, JK_STATUS_ARG_ATTRIBUTE) && attribute) {
            continue;
        }
        if (!strcmp(k, JK_STATUS_ARG_ATTRIBUTE) && cmd != JK_STATUS_CMD_UPDATE && cmd != JK_STATUS_CMD_EDIT) {
            continue;
        }
        if (!strncmp(k, JK_STATUS_ARG_MULT_VALUE_BASE, 3) && cmd != JK_STATUS_CMD_UPDATE) {
            continue;
        }
        if (k[0] == 'v' && cmd != JK_STATUS_CMD_UPDATE) {
            continue;
        }
        if (!strcmp(k, JK_STATUS_ARG_OPTIONS)) {
            opt = atoi(v);
            continue;
        }
        jk_printf(s, "%s%s=%s", started ? "&amp;" : "?", k, v);
        started=1;
    }
    if (opt | add_options | rm_options)
        jk_printf(s, "%s%s=%u", started ? "&amp;" : "?",
                  JK_STATUS_ARG_OPTIONS, (opt | add_options) & ~rm_options);
    if (text)
        jk_putv(s, "\">", text, "</a>", NULL);
}

static int status_parse_uri(jk_ws_service_t *s,
                            status_endpoint_t *p,
                            jk_logger_t *l)
{
    jk_map_t *m;
    status_worker_t *w = p->worker;
#ifdef _MT_CODE_PTHREAD
    char *lasts;
#endif
    char *param;
    char *query;

    JK_TRACE_ENTER(l);

    if (!s->query_string) {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' query string is empty",
                   w->name);
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    p->query_string = jk_pool_strdup(s->pool, s->query_string);
    if (!p->query_string) {
        jk_log(l, JK_LOG_ERROR,
               "Status worker '%s' could not copy query string",
               w->name);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    /* XXX We simply mask special chars n the query string with '@' to prevent cross site scripting */
    query = p->query_string;
    while ((query = strpbrk(query, JK_STATUS_ESC_CHARS)))
        query[0] = '@';

    if (!jk_map_alloc(&(p->req_params))) {
        jk_log(l, JK_LOG_ERROR,
               "Status worker '%s' could not alloc map for request parameters",
               w->name);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    m = p->req_params;

    query = jk_pool_strdup(s->pool, p->query_string);
    if (!query) {
        jk_log(l, JK_LOG_ERROR,
               "Status worker '%s' could not copy query string",
               w->name);
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

#ifdef _MT_CODE_PTHREAD
    for (param = strtok_r(query, "&", &lasts);
         param; param = strtok_r(NULL, "&", &lasts)) {
#else
    for (param = strtok(query, "&"); param; param = strtok(NULL, "&")) {
#endif
        char *key = jk_pool_strdup(s->pool, param);
        char *value;
        if (!key) {
            jk_log(l, JK_LOG_ERROR,
                   "Status worker '%s' could not copy string",
                   w->name);
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
        value = strchr(key, '=');
        if (value) {
            *value = '\0';
            value++;
    /* XXX Depending on the params values, we might need to trim and decode */
            if (strlen(key)) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Status worker '%s' adding request param '%s' with value '%s'",
                           w->name, key, value);
                jk_map_put(m, key, value, NULL);
            }
        }
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static void write_html_refresh_response(jk_ws_service_t *s,
                                        status_endpoint_t *p,
                                        const char *err,
                                        jk_logger_t *l)
{
    if (!err) {
        jk_puts(s, "\n<meta http-equiv=\"Refresh\" content=\""
                JK_STATUS_WAIT_AFTER_UPDATE ";url=");
        status_write_uri(s, p, NULL, JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                         NULL, NULL, 0, 0, NULL, l);
        jk_puts(s, "\">");
        jk_putv(s, "<p><b>Result: OK - You will be redirected in "
                JK_STATUS_WAIT_AFTER_UPDATE " seconds.</b><p/>", NULL);
    }
}

static int fetch_worker_and_sub_worker(status_endpoint_t *p,
                                       const char *operation,
                                       const char **worker,
                                       const char **sub_worker,
                                       jk_logger_t *l)
{
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    status_get_string(p, JK_STATUS_ARG_WORKER, NULL, worker, l);
    status_get_string(p, JK_STATUS_ARG_SUB_WORKER, NULL, sub_worker, l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Status worker '%s' %s worker '%s' sub worker '%s'",
               w->name, operation,
               *worker ? *worker : "(null)", *sub_worker ? *sub_worker : "(null)");
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int check_valid_lb(jk_ws_service_t *s,
                          status_endpoint_t *p,
                          jk_worker_t *jw,
                          const char *worker,
                          lb_worker_t **lbp,
                          int implemented,
                          jk_logger_t *l)
{
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    if (jw->type != JK_LB_WORKER_TYPE) {
        if (implemented) {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' worker type of worker '%s' has no sub workers",
                   w->name, worker);
            p->msg = "worker type has no sub workers";
        }
        else {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' worker type of worker '%s' not implemented",
                   w->name, worker);
                   p->msg = "worker type not implemented";
        }
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    *lbp = (lb_worker_t *)jw->worker_private;
    if (!*lbp) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' lb structure of worker '%s' is (null)",
               w->name, worker);
        p->msg = "lb structure is (null)";
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    p->msg = "OK";
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int search_worker(jk_ws_service_t *s,
                         status_endpoint_t *p,
                         jk_worker_t **jwp,
                         const char *worker,
                         jk_logger_t *l)
{
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    *jwp = NULL;
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Status worker '%s' searching worker '%s'",
               w->name, worker ? worker : "(null)");
    if (!worker || !worker[0]) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' NULL or EMPTY worker param",
               w->name);
        p->msg = "NULL or EMPTY worker param";
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    *jwp = wc_get_worker_for_name(worker, l);
    if (!*jwp) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' could not find worker '%s'",
               w->name, worker);
        p->msg = "Could not find given worker";
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    p->msg = "OK";
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int search_sub_worker(jk_ws_service_t *s,
                             status_endpoint_t *p,
                             jk_worker_t *jw,
                             const char *worker,
                             lb_sub_worker_t **wrp,
                             const char *sub_worker,
                             unsigned int *idx,
                             jk_logger_t *l)
{
    lb_worker_t *lb = NULL;
    lb_sub_worker_t *wr = NULL;
    status_worker_t *w = p->worker;
    unsigned int i = 0;

    JK_TRACE_ENTER(l);
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Status worker '%s' searching sub worker '%s' of worker '%s'",
               w->name, sub_worker ? sub_worker : "(null)",
               worker ? worker : "(null)");
    if (!sub_worker || !sub_worker[0]) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' NULL or EMPTY sub_worker param",
               w->name);
        p->msg = "NULL or EMPTY sub_worker param";
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    if (check_valid_lb(s, p, jw, worker, &lb, 1, l) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    if (idx)
        i = *idx;
    for (; i < lb->num_of_workers; i++) {
        wr = &(lb->lb_workers[i]);
        if (idx) {
            if (jk_wildchar_match(wr->name, sub_worker, 0) == 0) {
                *idx = i + 1;
                break;
            }
        }
        else if (strcmp(sub_worker, wr->name) == 0)
            break;
    }
    *wrp = wr;
    if (!wr || i == lb->num_of_workers) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' could not find sub worker '%s' of worker '%s'",
               w->name, sub_worker, worker ? worker : "(null)");
        p->msg = "could not find sub worker";
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    p->msg = "OK";
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int count_map(jk_uri_worker_map_t *uw_map,
                      const char *worker,
                      jk_logger_t *l)
{
    unsigned int i;
    int count=0;

    JK_TRACE_ENTER(l);
    if (uw_map) {
        for (i = 0; i < uw_map->size[uw_map->index]; i++) {
            uri_worker_record_t *uwr = uw_map->maps[uw_map->index][i];
            if (strcmp(uwr->worker_name, worker) &&
                strcmp(uwr->worker_name, "*")) {
                continue;
            }
            count++;
        }
    }
    JK_TRACE_EXIT(l);
    return count;
}

static int count_maps(jk_ws_service_t *s,
                      const char *worker,
                      jk_logger_t *l)
{
    int count=0;

    JK_TRACE_ENTER(l);
    if (s->next_vhost) {
        void *srv;
        for (srv = s->next_vhost(NULL); srv; srv = s->next_vhost(srv)) {
            count += count_map(s->vhost_to_uw_map(srv), worker, l);
        }
    }
    else if (s->uw_map)
        count = count_map(s->uw_map, worker, l);
    JK_TRACE_EXIT(l);
    return count;
}

static void display_map(jk_ws_service_t *s,
                        status_endpoint_t *p,
                        jk_uri_worker_map_t *uw_map,
                        const char *worker,
                        const char *server_name,
                        int *count_ptr,
                        int mime,
                        jk_logger_t *l)
{
    char buf[64];
    unsigned int i;
    int count;
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);

    if (uw_map->fname) {
        uri_worker_map_update(uw_map, 1, l);
    }
    for (i = 0; i < uw_map->size[uw_map->index]; i++) {
        uri_worker_record_t *uwr = uw_map->maps[uw_map->index][i];

        if (strcmp(uwr->worker_name, worker) &&
            strcmp(uwr->worker_name, "*")) {
            continue;
        }
        (*count_ptr)++;
        count = *count_ptr;

        if (mime == JK_STATUS_MIME_HTML) {
            if (server_name)
                jk_printf(s, JK_STATUS_URI_MAP_TABLE_ROW2,
                          server_name,
                          uwr->uri,
                          uri_worker_map_get_match(uwr, buf, l),
                          uri_worker_map_get_source(uwr, l),
                          uwr->extensions.reply_timeout,
                          uwr->extensions.fail_on_status_str ? uwr->extensions.fail_on_status_str : "-",
                          uwr->extensions.active ? uwr->extensions.active : "-",
                          uwr->extensions.disabled ? uwr->extensions.disabled : "-",
                          uwr->extensions.stopped ? uwr->extensions.stopped : "-",
                          uwr->extensions.use_server_error_pages);
            else
                jk_printf(s, JK_STATUS_URI_MAP_TABLE_ROW,
                          uwr->uri,
                          uri_worker_map_get_match(uwr, buf, l),
                          uri_worker_map_get_source(uwr, l),
                          uwr->extensions.reply_timeout,
                          uwr->extensions.fail_on_status_str ? uwr->extensions.fail_on_status_str : "-",
                          uwr->extensions.active ? uwr->extensions.active : "-",
                          uwr->extensions.disabled ? uwr->extensions.disabled : "-",
                          uwr->extensions.stopped ? uwr->extensions.stopped : "-",
                          uwr->extensions.use_server_error_pages);
        }
        else if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_start_elt(s, w, 6, 0, "map");
            jk_print_xml_att_int(s, 8, "id", count);
            if (server_name)
                jk_print_xml_att_string(s, 8, "server", server_name);
            jk_print_xml_att_string(s, 8, "uri", uwr->uri);
            jk_print_xml_att_string(s, 8, "type", uri_worker_map_get_match(uwr, buf, l));
            jk_print_xml_att_string(s, 8, "source", uri_worker_map_get_source(uwr, l));
            jk_print_xml_att_int(s, 8, "reply_timeout", uwr->extensions.reply_timeout);
            jk_print_xml_att_string(s, 8, "fail_on_status", uwr->extensions.fail_on_status_str);
            jk_print_xml_att_string(s, 8, "active", uwr->extensions.active);
            jk_print_xml_att_string(s, 8, "disabled", uwr->extensions.disabled);
            jk_print_xml_att_string(s, 8, "stopped", uwr->extensions.stopped);
            jk_print_xml_att_int(s, 8, "use_server_errors", uwr->extensions.use_server_error_pages);
            jk_print_xml_stop_elt(s, 6, 1);
        }
        else if (mime == JK_STATUS_MIME_TXT) {
            jk_puts(s, "Map:");
            jk_printf(s, " id=%d", count);
            if (server_name)
                jk_printf(s, " server=\"%s\"", server_name);
            jk_printf(s, " uri=\"%s\"", uwr->uri);
            jk_printf(s, " type=\"%s\"", uri_worker_map_get_match(uwr, buf, l));
            jk_printf(s, " source=\"%s\"", uri_worker_map_get_source(uwr, l));
            jk_printf(s, " reply_timeout=\"%d\"", uwr->extensions.reply_timeout);
            jk_printf(s, " fail_on_status=\"%s\"", uwr->extensions.fail_on_status_str ? uwr->extensions.fail_on_status_str : "");
            jk_printf(s, " active=\"%s\"", uwr->extensions.active ? uwr->extensions.active : "");
            jk_printf(s, " disabled=\"%s\"", uwr->extensions.disabled ? uwr->extensions.disabled : "");
            jk_printf(s, " stopped=\"%s\"", uwr->extensions.stopped ? uwr->extensions.stopped : "");
            jk_printf(s, " use_server_errors=\"%d\"", uwr->extensions.use_server_error_pages);
            jk_puts(s, "\n");
        }
        else if (mime == JK_STATUS_MIME_PROP) {
            if (server_name)
               jk_print_prop_item_string(s, w, worker, "map", count, "server", server_name);
            jk_print_prop_item_string(s, w, worker, "map", count, "uri", uwr->uri);
            jk_print_prop_item_string(s, w, worker, "map", count, "type", uri_worker_map_get_match(uwr, buf, l));
            jk_print_prop_item_string(s, w, worker, "map", count, "source", uri_worker_map_get_source(uwr, l));
            jk_print_prop_item_int(s, w, worker, "map", count, "reply_timeout", uwr->extensions.reply_timeout);
            jk_print_prop_item_string(s, w, worker, "map", count, "fail_on_status", uwr->extensions.fail_on_status_str);
            jk_print_prop_item_string(s, w, worker, "map", count, "active", uwr->extensions.active);
            jk_print_prop_item_string(s, w, worker, "map", count, "disabled", uwr->extensions.disabled);
            jk_print_prop_item_string(s, w, worker, "map", count, "stopped", uwr->extensions.stopped);
            jk_print_prop_item_int(s, w, worker, "map", count, "use_server_errors", uwr->extensions.use_server_error_pages);
        }
    }
    JK_TRACE_EXIT(l);
}

static void display_maps(jk_ws_service_t *s,
                         status_endpoint_t *p,
                         const char *worker,
                         jk_logger_t *l)
{
    int mime;
    unsigned int hide;
    int has_server_iterator = 0;
    int count=0;
    const char *arg;
    status_worker_t *w = p->worker;
    jk_uri_worker_map_t *uw_map;
    char server_name[80];
    void *srv;

    JK_TRACE_ENTER(l);
    status_get_string(p, JK_STATUS_ARG_MIME, NULL, &arg, l);
    mime = status_mime_int(arg);
    hide = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                          JK_STATUS_ARG_OPTION_NO_MAPS;
    if (s->next_vhost)
        has_server_iterator = 1;

    count = count_maps(s, worker, l);

    if (hide) {
        if (count && mime == JK_STATUS_MIME_HTML) {
            jk_puts(s, "<p>\n");
            status_write_uri(s, p, "Show URI Mappings", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                             NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_MAPS, NULL, l);
            jk_puts(s, "</p>\n");
        }
        JK_TRACE_EXIT(l);
        return;
    }

    if (count) {
        if (mime == JK_STATUS_MIME_HTML) {
            jk_printf(s, "<hr/><h3>URI Mappings for %s (%d maps) [", worker, count);
            status_write_uri(s, p, "Hide", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                             NULL, NULL, JK_STATUS_ARG_OPTION_NO_MAPS, 0, NULL, l);
            jk_puts(s, "]</h3><table>\n");
            if (has_server_iterator)
                jk_printf(s, JK_STATUS_URI_MAP_TABLE_HEAD2,
                          "Server", "URI", "Match Type", "Source", "Reply Timeout", "Fail on Status", "Active", "Disabled", "Stopped", "Use Server Errors");
            else
                jk_printf(s, JK_STATUS_URI_MAP_TABLE_HEAD,
                          "URI", "Match Type", "Source", "Reply Timeout", "Fail on Status", "Active", "Disabled", "Stopped", "Use Server Errors");
        }
        count = 0;
        if (has_server_iterator) {
            for (srv = s->next_vhost(NULL); srv; srv = s->next_vhost(srv)) {
                uw_map = s->vhost_to_uw_map(srv);
                if (uw_map) {
                    s->vhost_to_text(srv, server_name, 80);
                    display_map(s, p, uw_map, worker, server_name, &count, mime, l);
                }
            }
        }
        else {
            uw_map = s->uw_map;
            if (uw_map) {
                display_map(s, p, uw_map, worker, NULL, &count, mime, l);
            }
        }
        if (mime == JK_STATUS_MIME_HTML) {
            jk_puts(s, "</table>\n");
        }
    }
    else {
        if (mime == JK_STATUS_MIME_HTML) {
            jk_putv(s, "<hr/><h3>Warning: No URI Mappings defined for ",
                    worker, " !</h3>\n", NULL);
        }
    }
    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Status worker '%s' displayed %d maps for worker '%s'",
               w->name, count, worker);
    JK_TRACE_EXIT(l);
}

static const char *dump_ajp_addr(ajp_worker_t *aw, char *buf)
{
    if (aw->port > 0)
        return jk_dump_hinfo(&aw->worker_inet_addr, buf);
    else {
        if (aw->addr_sequence != aw->s->addr_sequence)
            return "unresolved";
        else
            return "invalid";
    }
}

static void display_worker_ajp_conf_details(jk_ws_service_t *s,
                                            status_endpoint_t *p,
                                            ajp_worker_t *aw,
                                            int is_member,
                                            int type,
                                            jk_logger_t *l)
{
    char buf[32];

    JK_TRACE_ENTER(l);

    if (is_member)
        jk_printf(s, JK_STATUS_SHOW_MEMBER_CONF_ROW,
                  aw->name,
                  status_worker_type(type),
                  aw->host,
                  dump_ajp_addr(aw, buf),
                  aw->cache_timeout,
                  aw->connect_timeout,
                  aw->prepost_timeout,
                  aw->reply_timeout,
                  aw->retries,
                  aw->recovery_opts,
                  aw->max_packet_size);
    else
        jk_printf(s, JK_STATUS_SHOW_AJP_CONF_ROW,
                  status_worker_type(type),
                  aw->host,
                  dump_ajp_addr(aw, buf),
                  aw->cache_timeout,
                  aw->connect_timeout,
                  aw->prepost_timeout,
                  aw->reply_timeout,
                  aw->retries,
                  aw->recovery_opts,
                  aw->max_packet_size);
    JK_TRACE_EXIT(l);

}

static void display_worker_ajp_details(jk_ws_service_t *s,
                                       status_endpoint_t *p,
                                       ajp_worker_t *aw,
                                       lb_sub_worker_t *wr,
                                       lb_worker_t *lb,
                                       int ms_min,
                                       int ms_max,
                                       int map_count,
                                       jk_logger_t *l)
{
    char buf[32];
    char buf_rd[32];
    char buf_rd_sec[32];
    char buf_wr[32];
    char buf_wr_sec[32];
    int mime;
    const char *arg;
    time_t now = time(NULL);
    const char *name = NULL;
    const char *sub_name = NULL;
    const char *ajp_name = NULL;
    status_worker_t *w = p->worker;
    int rs_min = 0;
    int rs_max = 0;
    int delta_reset = (int)difftime(now, aw->s->last_reset);
    int delta_error = -1;
    char buf_time[JK_STATUS_TIME_BUF_SZ];
    char buf_tz[JK_STATUS_TIME_BUF_SZ];
    time_t error_time = 0;
    int rc_time = -1;

    JK_TRACE_ENTER(l);

    status_get_string(p, JK_STATUS_ARG_MIME, NULL, &arg, l);
    mime = status_mime_int(arg);

    if (lb) {
        name = lb->name;
        sub_name = wr->name;
        ajp_name = wr->name;
        error_time = wr->s->error_time;
        if (wr->s->state == JK_LB_STATE_ERROR) {
            rs_min = lb->recover_wait_time - (int)difftime(now, wr->s->error_time);
            if (rs_min < 0) {
                rs_min = 0;
            }
            rs_max = rs_min + lb->maintain_time;
            if (rs_min < ms_min) {
                rs_min = ms_min;
            }
        }
    }
    else {
        name = aw->name;
        sub_name = NULL;
        ajp_name = aw->name;
        error_time = aw->s->error_time;
    }

    if (error_time > 0) {
        delta_error = (int)difftime(now, error_time);
        rc_time = status_strftime(error_time, mime, buf_time, buf_tz, l);
    }

    if (mime == JK_STATUS_MIME_HTML) {

        if (lb)
            jk_printf(s, JK_STATUS_SHOW_MEMBER_ROW,
                      sub_name,
                      jk_lb_get_activation(wr, l),
                      jk_lb_get_state(wr, l),
                      wr->distance,
                      wr->lb_factor,
                      wr->lb_mult,
                      wr->s->lb_value,
                      aw->s->used,
                      delta_reset > 0 ? (int)(aw->s->used / delta_reset) : -1,
                      wr->s->errors,
                      aw->s->client_errors,
                      aw->s->reply_timeouts,
                      status_strfsize(aw->s->transferred, buf_wr),
                      delta_reset > 0 ? status_strfsize(aw->s->transferred / delta_reset, buf_wr_sec) : "-",
                      status_strfsize(aw->s->readed, buf_rd),
                      delta_reset > 0 ? status_strfsize(aw->s->readed / delta_reset , buf_rd_sec) : "-",
                      aw->s->busy,
                      aw->s->max_busy,
                      wr->route,
                      wr->redirect ? (*wr->redirect ? wr->redirect : "&nbsp;") : "&nbsp",
                      wr->domain ? (*wr->domain ? wr->domain : "&nbsp;") : "&nbsp",
                      rs_min,
                      rs_max,
                      delta_reset,
                      rc_time > 0 ? buf_time : "&nbsp;");
        else {
            jk_printf(s, JK_STATUS_SHOW_AJP_ROW,
                      jk_ajp_get_state(aw, l),
                      aw->s->used,
                      delta_reset > 0 ? (int)(aw->s->used / delta_reset) : -1,
                      aw->s->errors,
                      aw->s->client_errors,
                      aw->s->reply_timeouts,
                      status_strfsize(aw->s->transferred, buf_wr),
                      delta_reset > 0 ? status_strfsize(aw->s->transferred / delta_reset, buf_wr_sec) : "-",
                      status_strfsize(aw->s->readed, buf_rd),
                      delta_reset > 0 ? status_strfsize(aw->s->readed / delta_reset , buf_rd_sec) : "-",
                      aw->s->busy,
                      aw->s->max_busy,
                      delta_reset,
                      rc_time > 0 ? buf_time : "&nbsp;");
        }

    }
    else if (mime == JK_STATUS_MIME_XML) {

        int off = 2;
        if (lb)
            off = 6;
        if (lb) {
            jk_print_xml_start_elt(s, w, off, 0, "member");
            jk_print_xml_att_string(s, off+2, "name", sub_name);
            jk_print_xml_att_string(s, off+2, "type", status_worker_type(wr->worker->type));
        }
        else {
            jk_print_xml_start_elt(s, w, off, 0, "ajp");
            jk_print_xml_att_string(s, off+2, "name", ajp_name);
            jk_print_xml_att_string(s, off+2, "type", status_worker_type(aw->worker.type));
        }
        jk_print_xml_att_string(s, off+2, "host", aw->host);
        jk_print_xml_att_int(s, off+2, "port", aw->port);
        jk_print_xml_att_string(s, off+2, "address", dump_ajp_addr(aw, buf));
        jk_print_xml_att_int(s, off+2, "connection_pool_timeout", aw->cache_timeout);
        jk_print_xml_att_int(s, off+2, "ping_timeout", aw->ping_timeout);
        jk_print_xml_att_int(s, off+2, "connect_timeout", aw->connect_timeout);
        jk_print_xml_att_int(s, off+2, "prepost_timeout", aw->prepost_timeout);
        jk_print_xml_att_int(s, off+2, "reply_timeout", aw->reply_timeout);
        jk_print_xml_att_int(s, off+2, "connection_ping_interval", aw->conn_ping_interval);
        jk_print_xml_att_int(s, off+2, "retries", aw->retries);
        jk_print_xml_att_uint(s, off+2, "recovery_options", aw->recovery_opts);
        jk_print_xml_att_uint(s, off+2, "max_packet_size", aw->max_packet_size);
        if (lb) {
            jk_print_xml_att_string(s, off+2, "activation", jk_lb_get_activation(wr, l));
            jk_print_xml_att_int(s, off+2, "lbfactor", wr->lb_factor);
            jk_print_xml_att_string(s, off+2, "route", wr->route);
            jk_print_xml_att_string(s, off+2, "redirect", wr->redirect);
            jk_print_xml_att_string(s, off+2, "domain", wr->domain);
            jk_print_xml_att_int(s, off+2, "distance", wr->distance);
            jk_print_xml_att_string(s, off+2, "state", jk_lb_get_state(wr, l));
            jk_print_xml_att_uint64(s, off+2, "lbmult", wr->lb_mult);
            jk_print_xml_att_uint64(s, off+2, "lbvalue", wr->s->lb_value);
            jk_print_xml_att_uint64(s, off+2, "elected", aw->s->used);
            jk_print_xml_att_uint32(s, off+2, "errors", wr->s->errors);
        }
        else {
            jk_print_xml_att_uint64(s, off+2, "used", aw->s->used);
            jk_print_xml_att_uint32(s, off+2, "errors", aw->s->errors);
        }
        jk_print_xml_att_uint32(s, off+2, "client_errors", aw->s->client_errors);
        jk_print_xml_att_uint32(s, off+2, "reply_timeouts", aw->s->reply_timeouts);
        jk_print_xml_att_uint64(s, off+2, "transferred", aw->s->transferred);
        jk_print_xml_att_uint64(s, off+2, "read", aw->s->readed);
        jk_print_xml_att_int(s, off+2, "busy", aw->s->busy);
        jk_print_xml_att_int(s, off+2, "max_busy", aw->s->max_busy);
        if (lb) {
            jk_print_xml_att_int(s, off+2, "time_to_recover_min", rs_min);
            jk_print_xml_att_int(s, off+2, "time_to_recover_max", rs_max);
        }
        else
            jk_print_xml_att_int(s, off+2, "map_count", map_count);
        jk_print_xml_att_long(s, off+2, "last_reset_at", (long)aw->s->last_reset);
        jk_print_xml_att_int(s, off+2, "last_reset_ago", delta_reset);
        if (rc_time > 0 ) {
            jk_print_xml_att_string(s, off+2, "error_time_datetime", buf_time);
            jk_print_xml_att_string(s, off+2, "error_time_tz", buf_tz);
            jk_print_xml_att_int(s, off+2, "error_time_unix_seconds", error_time);
            jk_print_xml_att_int(s, off+2, "error_time_ago", delta_error);
        }
        /* Terminate the tag */
        jk_print_xml_stop_elt(s, off, 1);

    }
    else if (mime == JK_STATUS_MIME_TXT) {

        if (lb) {
            jk_puts(s, "Member:");
            jk_printf(s, " name=%s", sub_name);
            jk_printf(s, " type=%s", status_worker_type(wr->worker->type));
        }
        else {
            jk_puts(s, "AJP Worker:");
            jk_printf(s, " name=%s", ajp_name);
            jk_printf(s, " type=%s", status_worker_type(aw->worker.type));
        }
        jk_printf(s, " host=%s", aw->host);
        jk_printf(s, " port=%d", aw->port);
        jk_printf(s, " address=%s", dump_ajp_addr(aw, buf));
        jk_printf(s, " connection_pool_timeout=%d", aw->cache_timeout);
        jk_printf(s, " ping_timeout=%d", aw->ping_timeout);
        jk_printf(s, " connect_timeout=%d", aw->connect_timeout);
        jk_printf(s, " prepost_timeout=%d", aw->prepost_timeout);
        jk_printf(s, " reply_timeout=%d", aw->reply_timeout);
        jk_printf(s, " retries=%d", aw->retries);
        jk_printf(s, " connection_ping_interval=%d", aw->conn_ping_interval);
        jk_printf(s, " recovery_options=%u", aw->recovery_opts);
        jk_printf(s, " max_packet_size=%u", aw->max_packet_size);
        if (lb) {
            jk_printf(s, " activation=%s", jk_lb_get_activation(wr, l));
            jk_printf(s, " lbfactor=%d", wr->lb_factor);
            jk_printf(s, " route=\"%s\"", wr->route ? wr->route : "");
            jk_printf(s, " redirect=\"%s\"", wr->redirect ? wr->redirect : "");
            jk_printf(s, " domain=\"%s\"", wr->domain ? wr->domain : "");
            jk_printf(s, " distance=%d", wr->distance);
            jk_printf(s, " state=%s", jk_lb_get_state(wr, l));
            jk_printf(s, " lbmult=%" JK_UINT64_T_FMT, wr->lb_mult);
            jk_printf(s, " lbvalue=%" JK_UINT64_T_FMT, wr->s->lb_value);
            jk_printf(s, " elected=%" JK_UINT64_T_FMT, aw->s->used);
            jk_printf(s, " errors=%" JK_UINT32_T_FMT, wr->s->errors);
        }
        else {
            jk_printf(s, " used=%" JK_UINT64_T_FMT, aw->s->used);
            jk_printf(s, " errors=%" JK_UINT32_T_FMT, aw->s->errors);
        }
        jk_printf(s, " client_errors=%" JK_UINT32_T_FMT, aw->s->client_errors);
        jk_printf(s, " reply_timeouts=%" JK_UINT32_T_FMT, aw->s->reply_timeouts);
        jk_printf(s, " transferred=%" JK_UINT64_T_FMT, aw->s->transferred);
        jk_printf(s, " read=%" JK_UINT64_T_FMT, aw->s->readed);
        jk_printf(s, " busy=%d", aw->s->busy);
        jk_printf(s, " max_busy=%d", aw->s->max_busy);
        if (lb) {
            jk_printf(s, " time_to_recover_min=%d", rs_min);
            jk_printf(s, " time_to_recover_max=%d", rs_max);
        }
        else
            jk_printf(s, " map_count=%d", map_count);
        jk_printf(s, " last_reset_at=%ld", (long)aw->s->last_reset);
        jk_printf(s, " last_reset_ago=%d", delta_reset);
        if (rc_time > 0) {
            jk_printf(s, " error_time_datetime=%s", buf_time);
            jk_printf(s, " error_time_tz=%s", buf_tz);
            jk_printf(s, " error_time_unix_seconds=%d", error_time);
            jk_printf(s, " error_time_ago=%d", delta_error);
        }
        jk_puts(s, "\n");

    }
    else if (mime == JK_STATUS_MIME_PROP) {

        if (lb) {
            jk_print_prop_att_string(s, w, name, "balance_workers", sub_name);
            jk_print_prop_att_string(s, w, ajp_name, "type", status_worker_type(wr->worker->type));
        }
        else {
            jk_print_prop_att_string(s, w, name, "list", ajp_name);
            jk_print_prop_att_string(s, w, ajp_name, "type", status_worker_type(aw->worker.type));
        }
        jk_print_prop_att_string(s, w, ajp_name, "host", aw->host);
        jk_print_prop_att_int(s, w, ajp_name, "port", aw->port);
        jk_print_prop_att_string(s, w, ajp_name, "address", dump_ajp_addr(aw, buf));
        jk_print_prop_att_int(s, w, ajp_name, "connection_pool_timeout", aw->cache_timeout);
        jk_print_prop_att_int(s, w, ajp_name, "ping_timeout", aw->ping_timeout);
        jk_print_prop_att_int(s, w, ajp_name, "connect_timeout", aw->connect_timeout);
        jk_print_prop_att_int(s, w, ajp_name, "prepost_timeout", aw->prepost_timeout);
        jk_print_prop_att_int(s, w, ajp_name, "reply_timeout", aw->reply_timeout);
        jk_print_prop_att_int(s, w, ajp_name, "retries", aw->retries);
        jk_print_prop_att_int(s, w, ajp_name, "connection_ping_interval", aw->conn_ping_interval);
        jk_print_prop_att_uint(s, w, ajp_name, "recovery_options", aw->recovery_opts);
        jk_print_prop_att_uint(s, w, ajp_name, "max_packet_size", aw->max_packet_size);
        if (lb) {
            jk_print_prop_att_string(s, w, ajp_name, "activation", jk_lb_get_activation(wr, l));
            jk_print_prop_att_int(s, w, ajp_name, "lbfactor", wr->lb_factor);
            jk_print_prop_att_string(s, w, ajp_name, "route", wr->route);
            jk_print_prop_att_string(s, w, ajp_name, "redirect", wr->redirect);
            jk_print_prop_att_string(s, w, ajp_name, "domain", wr->domain);
            jk_print_prop_att_int(s, w, ajp_name, "distance", wr->distance);
            jk_print_prop_att_string(s, w, ajp_name, "state", jk_lb_get_state(wr, l));
            jk_print_prop_att_uint64(s, w, ajp_name, "lbmult", wr->lb_mult);
            jk_print_prop_att_uint64(s, w, ajp_name, "lbvalue", wr->s->lb_value);
            jk_print_prop_att_uint64(s, w, ajp_name, "elected", aw->s->used);
            jk_print_prop_att_uint32(s, w, ajp_name, "errors", wr->s->errors);
        }
        else
            jk_print_prop_att_uint64(s, w, ajp_name, "used", aw->s->used);
            jk_print_prop_att_uint32(s, w, ajp_name, "errors", aw->s->errors);
        jk_print_prop_att_uint32(s, w, ajp_name, "client_errors", aw->s->client_errors);
        jk_print_prop_att_uint32(s, w, ajp_name, "reply_timeouts", aw->s->reply_timeouts);
        jk_print_prop_att_uint64(s, w, ajp_name, "transferred", aw->s->transferred);
        jk_print_prop_att_uint64(s, w, ajp_name, "read", aw->s->readed);
        jk_print_prop_att_int(s, w, ajp_name, "busy", aw->s->busy);
        jk_print_prop_att_int(s, w, ajp_name, "max_busy", aw->s->max_busy);
        if (lb) {
            jk_print_prop_att_int(s, w, ajp_name, "time_to_recover_min", rs_min);
            jk_print_prop_att_int(s, w, ajp_name, "time_to_recover_max", rs_max);
        }
        else
            jk_print_prop_att_int(s, w, name, "map_count", map_count);
        jk_print_prop_att_long(s, w, name, "last_reset_at", (long)aw->s->last_reset);
        jk_print_prop_att_int(s, w, name, "last_reset_ago", delta_reset);
        if (rc_time > 0) {
            jk_print_prop_att_string(s, w, name, "error_time_datetime", buf_time);
            jk_print_prop_att_string(s, w, name, "error_time_tz", buf_tz);
            jk_print_prop_att_int(s, w, name, "error_time_unix seconds", error_time);
            jk_print_prop_att_int(s, w, name, "error_time_ago seconds", delta_error);
        }

    }
    JK_TRACE_EXIT(l);

}

static void display_worker_lb(jk_ws_service_t *s,
                              status_endpoint_t *p,
                              lb_worker_t *lb,
                              lb_sub_worker_t *swr,
                              jk_logger_t *l)
{
    int cmd;
    int mime;
    int read_only = 0;
    int single = 0;
    unsigned int hide_members;
    unsigned int hide_lb_conf;
    unsigned int hide_lb_summary;
    unsigned int hide_ajp_conf;
    const char *arg;
    time_t now = time(NULL);
    unsigned int good = 0;
    unsigned int degraded = 0;
    unsigned int bad = 0;
    int map_count;
    int ms_min;
    int ms_max;
    unsigned int j;
    int pstart = JK_FALSE;
    const char *name = lb->name;
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    status_get_string(p, JK_STATUS_ARG_CMD, NULL, &arg, l);
    cmd = status_cmd_int(arg);
    status_get_string(p, JK_STATUS_ARG_MIME, NULL, &arg, l);
    mime = status_mime_int(arg);
    hide_members = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                                  JK_STATUS_ARG_OPTION_NO_MEMBERS;
    hide_lb_conf = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                                  JK_STATUS_ARG_OPTION_NO_LB_CONF;
    hide_lb_summary = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                                     JK_STATUS_ARG_OPTION_NO_LB_SUMMARY;
    hide_ajp_conf = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                                   JK_STATUS_ARG_OPTION_NO_AJP_CONF;
    if (w->read_only) {
        read_only = 1;
    }
    else {
        read_only = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                    JK_STATUS_ARG_OPTION_READ_ONLY;
    }
    if (cmd == JK_STATUS_CMD_SHOW) {
        single = 1;
    }

    if (lb->sequence != lb->s->h.sequence)
        jk_lb_pull(lb, JK_FALSE, l);

    for (j = 0; j < lb->num_of_workers; j++) {
        lb_sub_worker_t *wr = &(lb->lb_workers[j]);
        int rate;
        rate = status_rate(wr, w, l);
        if (rate > 0 )
           good++;
        else if (rate < 0 )
           bad++;
        else
           degraded++;
    }

    map_count = count_maps(s, name, l);
    ms_min = lb->maintain_time - (int)difftime(now, lb->s->last_maintain_time);
    ms_max = ms_min + lb->maintain_time;
    ms_min -= JK_LB_MAINTAIN_TOLERANCE;
    if (ms_min < 0) {
        ms_min = 0;
    }
    if (ms_max < 0) {
        ms_max = 0;
    }

    if (mime == JK_STATUS_MIME_HTML) {

        jk_puts(s, "<hr/><h3>[");
        if (single) {
            jk_puts(s, "S");
        }
        else {
            status_write_uri(s, p, "S", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                             name, "", 0, 0, "", l);
        }
        if (!read_only) {
            jk_puts(s, "|");
            status_write_uri(s, p, "E", JK_STATUS_CMD_EDIT, JK_STATUS_MIME_UNKNOWN,
                             name, "", 0, 0, "", l);
            jk_puts(s, "|");
            status_write_uri(s, p, "R", JK_STATUS_CMD_RESET, JK_STATUS_MIME_UNKNOWN,
                             name, "", 0, 0, "", l);
        }
        jk_puts(s, "]&nbsp;&nbsp;");
        jk_putv(s, "Worker Status for ", name, "</h3>\n", NULL);
        if (hide_lb_conf) {
            pstart = JK_TRUE;
            jk_puts(s, "<p>\n");
            if (single) {
                status_write_uri(s, p, "Show LB Configuration", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_LB_CONF, "", l);
            }
            else {
                status_write_uri(s, p, "Show LB Configuration", JK_STATUS_CMD_LIST, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_LB_CONF, "", l);
            }
        }
        if (hide_lb_summary) {
            if (pstart == JK_FALSE)
                jk_puts(s, "<p>\n");
            else
                jk_puts(s, "&nbsp;&nbsp;|&nbsp;&nbsp;");
            pstart = JK_TRUE;
            if (single) {
                status_write_uri(s, p, "Show LB Summary", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_LB_SUMMARY, "", l);
            }
            else {
                status_write_uri(s, p, "Show LB Summary", JK_STATUS_CMD_LIST, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_LB_SUMMARY, "", l);
            }
        }
        if (!hide_members && hide_ajp_conf) {
            if (pstart == JK_FALSE)
                jk_puts(s, "<p>\n");
            else
                jk_puts(s, "&nbsp;&nbsp;|&nbsp;&nbsp;");
            pstart = JK_TRUE;
            if (single) {
                status_write_uri(s, p, "Show AJP Configuration", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_AJP_CONF, "", l);
            }
            else {
                status_write_uri(s, p, "Show AJP Configuration", JK_STATUS_CMD_LIST, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_AJP_CONF, "", l);
            }
        }
        if (hide_members) {
            if (pstart == JK_FALSE)
                jk_puts(s, "<p>\n");
            else
                jk_puts(s, "&nbsp;&nbsp;|&nbsp;&nbsp;");
            pstart = JK_TRUE;
            if (single) {
                status_write_uri(s, p, "Show Balancer Members", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_MEMBERS, "", l);
            }
            else {
                status_write_uri(s, p, "Show Balancer Members", JK_STATUS_CMD_LIST, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_MEMBERS, "", l);
            }
        }
        if (pstart == JK_TRUE)
            jk_puts(s, "</p>\n");

        if (!hide_lb_conf) {
            jk_puts(s, "<table>" JK_STATUS_SHOW_LB_HEAD);
            jk_puts(s, "[");
            status_write_uri(s, p, "Hide", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                             NULL, NULL, JK_STATUS_ARG_OPTION_NO_LB_CONF, 0, NULL, l);
            jk_puts(s, "]</th></tr>");
            jk_printf(s, JK_STATUS_SHOW_LB_ROW,
                      status_worker_type(JK_LB_WORKER_TYPE),
                      jk_get_bool(lb->sticky_session),
                      jk_get_bool(lb->sticky_session_force),
                      lb->retries,
                      jk_lb_get_method(lb, l),
                      jk_lb_get_lock(lb, l),
                      lb->recover_wait_time,
                      lb->error_escalation_time,
                      lb->max_reply_timeouts);
            jk_puts(s, "</table>\n<br/>\n");
        }

        if (!hide_lb_summary) {
            jk_puts(s, "<table><tr>"
                    "<th>Good</th><th>Degraded</th><th>Bad/Stopped</th><th>Busy</th><th>Max Busy</th><th>Next Maintenance</th><th>Last Reset</th><th>[");
            status_write_uri(s, p, "Hide", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                             NULL, NULL, JK_STATUS_ARG_OPTION_NO_LB_SUMMARY, 0, NULL, l);
            jk_puts(s, "]</th></tr>\n<tr>");
            jk_printf(s, "<td>%d</td>", good);
            jk_printf(s, "<td>%d</td>", degraded);
            jk_printf(s, "<td>%d</td>", bad);
            jk_printf(s, "<td>%d</td>", lb->s->busy);
            jk_printf(s, "<td>%d</td>", lb->s->max_busy);
            jk_printf(s, "<td>%d/%d</td>", ms_min, ms_max);
            jk_printf(s, "<td>%d</td>", (int)difftime(now, lb->s->last_reset));
            jk_puts(s, "<td></td></tr>\n</table>\n\n");
        }
    }
    else if (mime == JK_STATUS_MIME_XML) {

        jk_print_xml_start_elt(s, w, 2, 0, "balancer");
        jk_print_xml_att_string(s, 4, "name", name);
        jk_print_xml_att_string(s, 4, "type", status_worker_type(JK_LB_WORKER_TYPE));
        jk_print_xml_att_string(s, 4, "sticky_session", jk_get_bool(lb->sticky_session));
        jk_print_xml_att_string(s, 4, "sticky_session_force", jk_get_bool(lb->sticky_session_force));
        jk_print_xml_att_int(s, 4, "retries", lb->retries);
        jk_print_xml_att_int(s, 4, "recover_time", lb->recover_wait_time);
        jk_print_xml_att_int(s, 4, "error_escalation_time", lb->error_escalation_time);
        jk_print_xml_att_int(s, 4, "max_reply_timeouts", lb->max_reply_timeouts);
        jk_print_xml_att_string(s, 4, "method", jk_lb_get_method(lb, l));
        jk_print_xml_att_string(s, 4, "lock", jk_lb_get_lock(lb, l));
        jk_print_xml_att_int(s, 4, "member_count", lb->num_of_workers);
        jk_print_xml_att_int(s, 4, "good", good);
        jk_print_xml_att_int(s, 4, "degraded", degraded);
        jk_print_xml_att_int(s, 4, "bad", bad);
        jk_print_xml_att_int(s, 4, "busy", lb->s->busy);
        jk_print_xml_att_int(s, 4, "max_busy", lb->s->max_busy);
        jk_print_xml_att_int(s, 4, "map_count", map_count);
        jk_print_xml_att_int(s, 4, "time_to_maintenance_min", ms_min);
        jk_print_xml_att_int(s, 4, "time_to_maintenance_max", ms_max);
        jk_print_xml_att_long(s, 4, "last_reset_at", (long)lb->s->last_reset);
        jk_print_xml_att_int(s, 4, "last_reset_ago", (int)difftime(now, lb->s->last_reset));
        jk_print_xml_stop_elt(s, 2, 0);

    }
    else if (mime == JK_STATUS_MIME_TXT) {

        jk_puts(s, "Balancer Worker:");
        jk_printf(s, " name=%s", name);
        jk_printf(s, " type=%s", status_worker_type(JK_LB_WORKER_TYPE));
        jk_printf(s, " sticky_session=%s", jk_get_bool(lb->sticky_session));
        jk_printf(s, " sticky_session_force=%s", jk_get_bool(lb->sticky_session_force));
        jk_printf(s, " retries=%d", lb->retries);
        jk_printf(s, " recover_time=%d", lb->recover_wait_time);
        jk_printf(s, " error_escalation_time=%d", lb->error_escalation_time);
        jk_printf(s, " max_reply_timeouts=%d", lb->max_reply_timeouts);
        jk_printf(s, " method=%s", jk_lb_get_method(lb, l));
        jk_printf(s, " lock=%s", jk_lb_get_lock(lb, l));
        jk_printf(s, " member_count=%d", lb->num_of_workers);
        jk_printf(s, " good=%d", good);
        jk_printf(s, " degraded=%d", degraded);
        jk_printf(s, " bad=%d", bad);
        jk_printf(s, " busy=%d", lb->s->busy);
        jk_printf(s, " max_busy=%d", lb->s->max_busy);
        jk_printf(s, " map_count=%d", map_count);
        jk_printf(s, " time_to_maintenance_min=%d", ms_min);
        jk_printf(s, " time_to_maintenance_max=%d", ms_max);
        jk_printf(s, " last_reset_at=%ld", (long)lb->s->last_reset);
        jk_printf(s, " last_reset_ago=%d", (int)difftime(now, lb->s->last_reset));
        jk_puts(s, "\n");

    }
    else if (mime == JK_STATUS_MIME_PROP) {

        jk_print_prop_att_string(s, w, NULL, "list", name);
        jk_print_prop_att_string(s, w, name, "type", status_worker_type(JK_LB_WORKER_TYPE));
        jk_print_prop_att_string(s, w, name, "sticky_session", jk_get_bool(lb->sticky_session));
        jk_print_prop_att_string(s, w, name, "sticky_session_force", jk_get_bool(lb->sticky_session_force));
        jk_print_prop_att_int(s, w, name, "retries", lb->retries);
        jk_print_prop_att_int(s, w, name, "recover_time", lb->recover_wait_time);
        jk_print_prop_att_int(s, w, name, "error_escalation_time", lb->error_escalation_time);
        jk_print_prop_att_int(s, w, name, "max_reply_timeouts", lb->max_reply_timeouts);
        jk_print_prop_att_string(s, w, name, "method", jk_lb_get_method(lb, l));
        jk_print_prop_att_string(s, w, name, "lock", jk_lb_get_lock(lb, l));
        jk_print_prop_att_int(s, w, name, "member_count", lb->num_of_workers);
        jk_print_prop_att_int(s, w, name, "good", good);
        jk_print_prop_att_int(s, w, name, "degraded", degraded);
        jk_print_prop_att_int(s, w, name, "bad", bad);
        jk_print_prop_att_int(s, w, name, "busy", lb->s->busy);
        jk_print_prop_att_int(s, w, name, "max_busy", lb->s->max_busy);
        jk_print_prop_att_int(s, w, name, "map_count", map_count);
        jk_print_prop_att_int(s, w, name, "time_to_maintenance_min", ms_min);
        jk_print_prop_att_int(s, w, name, "time_to_maintenance_max", ms_max);
        jk_print_prop_att_long(s, w, name, "last_reset_at", (long)lb->s->last_reset);
        jk_print_prop_att_int(s, w, name, "last_reset_ago", (int)difftime(now, lb->s->last_reset));

    }

    if (!hide_members) {

        if (mime == JK_STATUS_MIME_HTML) {

            jk_puts(s, "<h4>Balancer Members [");
            if (swr) {
                status_write_uri(s, p, "Show All Members", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                 name, "", 0, 0, "", l);
                jk_puts(s, "]&nbsp;&nbsp;[");
            }
            if (single) {
                status_write_uri(s, p, "Hide", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, JK_STATUS_ARG_OPTION_NO_MEMBERS, 0, "", l);
            }
            else {
                status_write_uri(s, p, "Hide", JK_STATUS_CMD_LIST, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, JK_STATUS_ARG_OPTION_NO_MEMBERS, 0, "", l);
            }
            jk_puts(s, "]</h4>\n");
            if (!hide_ajp_conf) {
                jk_puts(s, "<table>" JK_STATUS_SHOW_MEMBER_CONF_HEAD);
                jk_puts(s, "[");
                status_write_uri(s, p, "Hide", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, JK_STATUS_ARG_OPTION_NO_AJP_CONF, 0, NULL, l);
                jk_puts(s, "]</td></tr>");
                if (swr) {
                    jk_worker_t *jw = (jk_worker_t *)swr->worker;
                    ajp_worker_t *aw = (ajp_worker_t *)jw->worker_private;
                    display_worker_ajp_conf_details(s, p, aw, 1, jw->type, l);
                }
                else
                    for (j = 0; j < lb->num_of_workers; j++) {
                        lb_sub_worker_t *wr = &(lb->lb_workers[j]);
                        jk_worker_t *jw = (jk_worker_t *)wr->worker;
                        ajp_worker_t *aw = (ajp_worker_t *)jw->worker_private;
                        display_worker_ajp_conf_details(s, p, aw, 1, jw->type, l);
                    }
                jk_puts(s, "</table>\n<br/>\n");
            }
            jk_puts(s, "<table>" JK_STATUS_SHOW_MEMBER_HEAD);

        }

        if (swr) {
            const char *sub_name = swr->name;
            ajp_worker_t *aw = (ajp_worker_t *)swr->worker->worker_private;

            if (mime == JK_STATUS_MIME_HTML) {
                jk_puts(s, "<tr>\n<td>[");
                jk_puts(s, "S");
                if (!read_only) {
                    jk_puts(s, "|");
                    status_write_uri(s, p, "E", JK_STATUS_CMD_EDIT, JK_STATUS_MIME_UNKNOWN,
                                     name, sub_name, 0, 0, "", l);
                    jk_puts(s, "|");
                    status_write_uri(s, p, "R", JK_STATUS_CMD_RESET, JK_STATUS_MIME_UNKNOWN,
                                     name, sub_name, 0, 0, "", l);
                    if (swr->s->state == JK_LB_STATE_ERROR) {
                        jk_puts(s, "|");
                        status_write_uri(s, p, "T", JK_STATUS_CMD_RECOVER, JK_STATUS_MIME_UNKNOWN,
                                         name, sub_name, 0, 0, "", l);
                    }
                }
                jk_puts(s, "]");
                jk_puts(s, "&nbsp;</td>");
            }
            display_worker_ajp_details(s, p, aw, swr, lb, ms_min, ms_max, 0, l);
        } else
            for (j = 0; j < lb->num_of_workers; j++) {
                lb_sub_worker_t *wr = &(lb->lb_workers[j]);
                const char *sub_name = wr->name;
                ajp_worker_t *aw = (ajp_worker_t *)wr->worker->worker_private;

                if (mime == JK_STATUS_MIME_HTML) {
                    jk_puts(s, "<tr>\n<td>[");
                    status_write_uri(s, p, "S", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                 name, sub_name, 0, 0, "", l);
                    if (!read_only) {
                        jk_puts(s, "|");
                        status_write_uri(s, p, "E", JK_STATUS_CMD_EDIT, JK_STATUS_MIME_UNKNOWN,
                                         name, sub_name, 0, 0, "", l);
                        jk_puts(s, "|");
                        status_write_uri(s, p, "R", JK_STATUS_CMD_RESET, JK_STATUS_MIME_UNKNOWN,
                                         name, sub_name, 0, 0, "", l);
                        if (wr->s->state == JK_LB_STATE_ERROR) {
                            jk_puts(s, "|");
                            status_write_uri(s, p, "T", JK_STATUS_CMD_RECOVER, JK_STATUS_MIME_UNKNOWN,
                                             name, sub_name, 0, 0, "", l);
                        }
                    }
                    jk_puts(s, "]");
                    jk_puts(s, "&nbsp;</td>");
                }
                display_worker_ajp_details(s, p, aw, wr, lb, ms_min, ms_max, 0, l);
            }

        if (mime == JK_STATUS_MIME_HTML) {

            jk_puts(s, "</table><br/>\n");
            if (!read_only) {
                const char *arg;
                status_get_string(p, JK_STATUS_ARG_CMD, NULL, &arg, l);
                status_start_form(s, p, "get", JK_STATUS_CMD_EDIT, NULL, l);
                jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING, JK_STATUS_ARG_WORKER, name);
                if (arg)
                    jk_printf(s, JK_STATUS_FORM_HIDDEN_STRING, JK_STATUS_ARG_FROM, arg);
                jk_puts(s, "<table><tr><td><b>E</b>dit this attribute for all members:</td><td>");
                jk_putv(s, "<select name=\"", JK_STATUS_ARG_ATTRIBUTE,
                        "\" size=\"1\">\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_LBM_ACTIVATION, "\">", JK_STATUS_ARG_LBM_TEXT_ACTIVATION, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_LBM_FACTOR, "\">", JK_STATUS_ARG_LBM_TEXT_FACTOR, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_LBM_ROUTE, "\">", JK_STATUS_ARG_LBM_TEXT_ROUTE, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_LBM_REDIRECT, "\">", JK_STATUS_ARG_LBM_TEXT_REDIRECT, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_LBM_DOMAIN, "\">", JK_STATUS_ARG_LBM_TEXT_DOMAIN, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_LBM_DISTANCE, "\">", JK_STATUS_ARG_LBM_TEXT_DISTANCE, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_CACHE_TO, "\">", JK_STATUS_ARG_AJP_TEXT_CACHE_TO, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_PING_TO, "\">", JK_STATUS_ARG_AJP_TEXT_PING_TO, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_CONNECT_TO, "\">", JK_STATUS_ARG_AJP_TEXT_CONNECT_TO, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_PREPOST_TO, "\">", JK_STATUS_ARG_AJP_TEXT_PREPOST_TO, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_REPLY_TO, "\">", JK_STATUS_ARG_AJP_TEXT_REPLY_TO, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_RETRIES, "\">", JK_STATUS_ARG_AJP_TEXT_RETRIES, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_RETRY_INT, "\">", JK_STATUS_ARG_AJP_TEXT_RETRY_INT, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_CPING_INT, "\">", JK_STATUS_ARG_AJP_TEXT_CPING_INT, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_REC_OPTS, "\">", JK_STATUS_ARG_AJP_TEXT_REC_OPTS, "</option>\n", NULL);
                jk_putv(s, "<option value=\"", JK_STATUS_ARG_AJP_MAX_PK_SZ, "\">", JK_STATUS_ARG_AJP_TEXT_MAX_PK_SZ, "</option>\n", NULL);
                jk_puts(s, "</select></td><td><input type=\"submit\" value=\"Go\"/></tr></table></form>\n");
            }

        }

    }

    if (name)
        display_maps(s, p, name, l);

    if (mime == JK_STATUS_MIME_XML) {
        jk_print_xml_close_elt(s, w, 2, "balancer");
    }

    JK_TRACE_EXIT(l);
}

static void display_worker_ajp(jk_ws_service_t *s,
                               status_endpoint_t *p,
                               ajp_worker_t *aw,
                               int type,
                               jk_logger_t *l)
{
    int cmd;
    int mime;
    int read_only = 0;
    int single = 0;
    unsigned int hide_ajp_conf;
    const char *arg;
    int map_count;
    const char *name = aw->name;
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    status_get_string(p, JK_STATUS_ARG_CMD, NULL, &arg, l);
    cmd = status_cmd_int(arg);
    status_get_string(p, JK_STATUS_ARG_MIME, NULL, &arg, l);
    mime = status_mime_int(arg);
    hide_ajp_conf = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                                   JK_STATUS_ARG_OPTION_NO_AJP_CONF;
    if (w->read_only) {
        read_only = 1;
    }
    else {
        read_only = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                    JK_STATUS_ARG_OPTION_READ_ONLY;
    }
    if (cmd == JK_STATUS_CMD_SHOW) {
        single = 1;
    }

    if (aw->sequence != aw->s->h.sequence)
        jk_ajp_pull(aw, JK_FALSE, l);

    map_count = count_maps(s, name, l);

    if (mime == JK_STATUS_MIME_HTML) {

        jk_puts(s, "<hr/><h3>[");
        if (single)
            jk_puts(s, "S");
        else
            status_write_uri(s, p, "S", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                             name, "", 0, 0, "", l);
        if (!read_only) {
            jk_puts(s, "|");
            status_write_uri(s, p, "E", JK_STATUS_CMD_EDIT, JK_STATUS_MIME_UNKNOWN,
                             name, "", 0, 0, "", l);
            jk_puts(s, "|");
            status_write_uri(s, p, "R", JK_STATUS_CMD_RESET, JK_STATUS_MIME_UNKNOWN,
                             name, "", 0, 0, "", l);
        }
        jk_puts(s, "]&nbsp;&nbsp;");
        jk_putv(s, "Worker Status for ", name, "</h3>\n", NULL);
        if (!hide_ajp_conf) {
            jk_puts(s, "<table>" JK_STATUS_SHOW_AJP_CONF_HEAD);
            jk_puts(s, "[");
            status_write_uri(s, p, "Hide", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                             NULL, NULL, JK_STATUS_ARG_OPTION_NO_AJP_CONF, 0, NULL, l);
            jk_puts(s, "]</td></tr>");
            display_worker_ajp_conf_details(s, p, aw, 0, type, l);
            jk_puts(s, "</table>\n<br/>\n");
        }
        else {
            jk_puts(s, "<p>\n");
            if (single) {
                status_write_uri(s, p, "Show AJP Configuration", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_AJP_CONF, "", l);
            }
            else {
                status_write_uri(s, p, "Show AJP Configuration", JK_STATUS_CMD_LIST, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_AJP_CONF, "", l);
            }
            jk_puts(s, "</p>\n");
        }
        jk_puts(s, "<table>" JK_STATUS_SHOW_AJP_HEAD);
    }
    display_worker_ajp_details(s, p, aw, NULL, NULL, 0, 0, map_count, l);

    if (mime == JK_STATUS_MIME_HTML) {
        jk_puts(s, "</table>\n");
    }
    if (name)
        display_maps(s, p, name, l);

    JK_TRACE_EXIT(l);
}

static void display_worker(jk_ws_service_t *s,
                           status_endpoint_t *p,
                           jk_worker_t *jw,
                           lb_sub_worker_t *swr,
                           jk_logger_t *l)
{
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    if (jw->type == JK_LB_WORKER_TYPE) {
        lb_worker_t *lb = (lb_worker_t *)jw->worker_private;
        if (lb) {
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s lb worker '%s'",
                       w->name, "displaying", lb->name);
            display_worker_lb(s, p, lb, swr, l);
        }
        else {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' lb worker is (null)",
                   w->name);
        }
    }
    else if (jw->type == JK_AJP13_WORKER_TYPE ||
             jw->type == JK_AJP14_WORKER_TYPE) {
        ajp_worker_t *aw = (ajp_worker_t *)jw->worker_private;
        if (aw) {
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s ajp worker '%s'",
                       w->name, "displaying", aw->name);
            display_worker_ajp(s, p, aw, jw->type, l);
        }
        else {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' aw worker is (null)",
                   w->name);
        }
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' worker type not implemented",
                   w->name);
        JK_TRACE_EXIT(l);
        return;
    }
}

static void form_worker(jk_ws_service_t *s,
                        status_endpoint_t *p,
                        jk_worker_t *jw,
                        jk_logger_t *l)
{
    const char *name = NULL;
    lb_worker_t *lb = NULL;
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    if (jw->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)jw->worker_private;
        name = lb->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' producing edit form for lb worker '%s'",
                   w->name, name);
    }
    else {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' worker type not implemented",
               w->name);
        JK_TRACE_EXIT(l);
        return;
    }

    if (!lb) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' lb structure is (null)",
               w->name);
        JK_TRACE_EXIT(l);
        return;
    }

    jk_putv(s, "<hr/><h3>Edit load balancer settings for ",
            name, "</h3>\n", NULL);

    status_start_form(s, p, "get", JK_STATUS_CMD_UPDATE, NULL, l);

    jk_putv(s, "<table>\n<tr><td>", JK_STATUS_ARG_LB_TEXT_RETRIES,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_RETRIES, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->retries);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_RETRY_INT,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_RETRY_INT, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->retry_interval);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_RECOVER_TIME,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_RECOVER_TIME, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->recover_wait_time);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_ERROR_ESCALATION_TIME,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_ERROR_ESCALATION_TIME, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->error_escalation_time);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_MAX_REPLY_TIMEOUTS,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_MAX_REPLY_TIMEOUTS, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", lb->max_reply_timeouts);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_STICKY,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_STICKY, "\" type=\"checkbox\"", NULL);
    if (lb->sticky_session)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_STICKY_FORCE,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_LB_STICKY_FORCE, "\" type=\"checkbox\"", NULL);
    if (lb->sticky_session_force)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_METHOD,
            ":</td><td></td></tr>\n", NULL);
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Requests</td><td><input name=\"",
            JK_STATUS_ARG_LB_METHOD, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_METHOD_REQUESTS);
    if (lb->lbmethod == JK_LB_METHOD_REQUESTS)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Traffic</td><td><input name=\"",
            JK_STATUS_ARG_LB_METHOD, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_METHOD_TRAFFIC);
    if (lb->lbmethod == JK_LB_METHOD_TRAFFIC)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Busyness</td><td><input name=\"",
            JK_STATUS_ARG_LB_METHOD, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_METHOD_BUSYNESS);
    if (lb->lbmethod == JK_LB_METHOD_BUSYNESS)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Sessions</td><td><input name=\"",
            JK_STATUS_ARG_LB_METHOD, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_METHOD_SESSIONS);
    if (lb->lbmethod == JK_LB_METHOD_SESSIONS)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_LB_TEXT_LOCK,
            ":</td><td></td></tr>\n", NULL);
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Optimistic</td><td><input name=\"",
            JK_STATUS_ARG_LB_LOCK, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_LOCK_OPTIMISTIC);
    if (lb->lblock == JK_LB_LOCK_OPTIMISTIC)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_putv(s, "<tr><td>&nbsp;&nbsp;Pessimistic</td><td><input name=\"",
            JK_STATUS_ARG_LB_LOCK, "\" type=\"radio\"", NULL);
    jk_printf(s, " value=\"%d\"", JK_LB_LOCK_PESSIMISTIC);
    if (lb->lblock == JK_LB_LOCK_PESSIMISTIC)
        jk_puts(s, " checked=\"checked\"");
    jk_puts(s, "/></td></tr>\n");
    jk_puts(s, "</table>\n");
    jk_puts(s, "<br/><input type=\"submit\" value=\"Update Balancer\"/></form>\n");

    JK_TRACE_EXIT(l);
}

static void form_member(jk_ws_service_t *s,
                        status_endpoint_t *p,
                        lb_sub_worker_t *wr,
                        ajp_worker_t *aw,
                        const char *lb_name,
                        jk_logger_t *l)
{
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);

    if (JK_IS_DEBUG_LEVEL(l))
        jk_log(l, JK_LOG_DEBUG,
               "Status worker '%s' producing edit form for sub worker '%s' of lb worker '%s'",
               w->name, wr? wr->name : aw->name, lb_name);

    jk_putv(s, "<hr/><h3>Edit worker settings for ",
            wr? wr->name : aw->name, "</h3>\n", NULL);
    status_start_form(s, p, "get", JK_STATUS_CMD_UPDATE, NULL, l);

    if (wr) {
        jk_puts(s, "<table><tbody valign=\"baseline\"><tr><th>Balancing related settings</th>\n");
        jk_puts(s, "<th>&nbsp;&nbsp;</th><th>AJP settings</th>\n");
        jk_puts(s, "</tr>\n");
        jk_puts(s, "<tr><td><table>\n");
        jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_ACTIVATION,
                ":</td><td></td></tr>\n", NULL);
        jk_putv(s, "<tr><td>&nbsp;&nbsp;Active</td><td><input name=\"",
                JK_STATUS_ARG_LBM_ACTIVATION, "\" type=\"radio\"", NULL);
        jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_ACTIVE);
        if (wr->activation == JK_LB_ACTIVATION_ACTIVE)
            jk_puts(s, " checked=\"checked\"");
        jk_puts(s, "/></td></tr>\n");
        jk_putv(s, "<tr><td>&nbsp;&nbsp;Disabled</td><td><input name=\"",
                JK_STATUS_ARG_LBM_ACTIVATION, "\" type=\"radio\"", NULL);
        jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_DISABLED);
        if (wr->activation == JK_LB_ACTIVATION_DISABLED)
            jk_puts(s, " checked=\"checked\"");
        jk_puts(s, "/></td></tr>\n");
        jk_putv(s, "<tr><td>&nbsp;&nbsp;Stopped</td><td><input name=\"",
                JK_STATUS_ARG_LBM_ACTIVATION, "\" type=\"radio\"", NULL);
        jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_STOPPED);
        if (wr->activation == JK_LB_ACTIVATION_STOPPED)
            jk_puts(s, " checked=\"checked\"");
        jk_puts(s, "/></td></tr>\n");
        jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_FACTOR,
                ":</td><td><input name=\"",
                JK_STATUS_ARG_LBM_FACTOR, "\" type=\"text\" ", NULL);
        jk_printf(s, "value=\"%d\"/></td></tr>\n", wr->lb_factor);
        jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_ROUTE,
                ":</td><td><input name=\"",
                JK_STATUS_ARG_LBM_ROUTE, "\" type=\"text\" ", NULL);
        jk_printf(s, "value=\"%s\"/></td></tr>\n", wr->route);
        jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_REDIRECT,
                ":</td><td><input name=\"",
                JK_STATUS_ARG_LBM_REDIRECT, "\" type=\"text\" ", NULL);
        jk_putv(s, "value=\"", wr->redirect, NULL);
        jk_puts(s, "\"/></td></tr>\n");
        jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_DOMAIN,
                ":</td><td><input name=\"",
                JK_STATUS_ARG_LBM_DOMAIN, "\" type=\"text\" ", NULL);
        jk_putv(s, "value=\"", wr->domain, NULL);
        jk_puts(s, "\"/></td></tr>\n");
        jk_putv(s, "<tr><td>", JK_STATUS_ARG_LBM_TEXT_DISTANCE,
                ":</td><td><input name=\"",
                JK_STATUS_ARG_LBM_DISTANCE, "\" type=\"text\" ", NULL);
        jk_printf(s, "value=\"%d\"/></td></tr>\n", wr->distance);
        jk_puts(s, "</table>\n");
        jk_puts(s, "</td><td></td><td>\n");
    }

    jk_puts(s, "<table>\n");
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_HOST_STR,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_HOST_STR, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%s\"/></td></tr>\n", aw->host);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_PORT,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_PORT, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->port);

    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_CACHE_TO,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_CACHE_TO, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->cache_timeout);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_PING_TO,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_PING_TO, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->ping_timeout);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_CONNECT_TO,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_CONNECT_TO, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->connect_timeout);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_PREPOST_TO,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_PREPOST_TO, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->prepost_timeout);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_REPLY_TO,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_REPLY_TO, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->reply_timeout);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_RETRIES,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_RETRIES, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->retries);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_RETRY_INT,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_RETRY_INT, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->retry_interval);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_CPING_INT,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_CPING_INT, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->conn_ping_interval);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_REC_OPTS,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_REC_OPTS, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->recovery_opts);
    jk_putv(s, "<tr><td>", JK_STATUS_ARG_AJP_TEXT_MAX_PK_SZ,
            ":</td><td><input name=\"",
            JK_STATUS_ARG_AJP_MAX_PK_SZ, "\" type=\"text\" ", NULL);
    jk_printf(s, "value=\"%d\"/></td></tr>\n", aw->max_packet_size);
    jk_puts(s, "</table>\n");
    if (wr)
        jk_puts(s, "</td></tr></table>\n");
    jk_puts(s, "<br/><input type=\"submit\" value=\"Update Worker\"/>\n</form>\n");
    JK_TRACE_EXIT(l);
}

static void form_all_members(jk_ws_service_t *s,
                             status_endpoint_t *p,
                             jk_worker_t *jw,
                             const char *attribute,
                             jk_logger_t *l)
{
    const char *name = NULL;
    lb_worker_t *lb = NULL;
    status_worker_t *w = p->worker;
    const char *aname;
    unsigned int i;

    JK_TRACE_ENTER(l);
    if (!attribute) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' missing request parameter '%s'",
               w->name, JK_STATUS_ARG_ATTRIBUTE);
        JK_TRACE_EXIT(l);
        return;
    }
    else {
        if (!strcmp(attribute, JK_STATUS_ARG_LBM_ACTIVATION))
            aname=JK_STATUS_ARG_LBM_TEXT_ACTIVATION;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_FACTOR))
            aname=JK_STATUS_ARG_LBM_TEXT_FACTOR;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_ROUTE))
            aname=JK_STATUS_ARG_LBM_TEXT_ROUTE;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_REDIRECT))
            aname=JK_STATUS_ARG_LBM_TEXT_REDIRECT;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DOMAIN))
            aname=JK_STATUS_ARG_LBM_TEXT_DOMAIN;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DISTANCE))
            aname=JK_STATUS_ARG_LBM_TEXT_DISTANCE;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CACHE_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_CACHE_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_PING_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_PING_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CONNECT_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_CONNECT_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_PREPOST_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_PREPOST_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_REPLY_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_REPLY_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_RETRIES))
            aname=JK_STATUS_ARG_AJP_TEXT_RETRIES;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_RETRY_INT))
            aname=JK_STATUS_ARG_AJP_TEXT_RETRY_INT;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CPING_INT))
            aname=JK_STATUS_ARG_AJP_TEXT_CPING_INT;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_REC_OPTS))
            aname=JK_STATUS_ARG_AJP_TEXT_REC_OPTS;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_MAX_PK_SZ))
            aname=JK_STATUS_ARG_AJP_TEXT_MAX_PK_SZ;
        else {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' unknown attribute '%s'",
                   w->name, attribute);
            JK_TRACE_EXIT(l);
            return;
        }
    }
    if (jw->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)jw->worker_private;
        name = lb->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' producing edit form for attribute '%s' [%s] of all members of lb worker '%s'",
                   w->name, attribute, aname, name);
    }
    else {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' worker type not implemented",
               w->name);
        JK_TRACE_EXIT(l);
        return;
    }

    if (lb) {
        jk_putv(s, "<hr/><h3>Edit attribute '", aname,
                "' for all members of load balancer ",
                name, "</h3>\n", NULL);

        status_start_form(s, p, "get", JK_STATUS_CMD_UPDATE, NULL, l);

        jk_putv(s, "<table><tr>"
                "<th>Balanced Worker</th><th>", aname, "</th>"
                "</tr>", NULL);

        for (i = 0; i < lb->num_of_workers; i++) {
            lb_sub_worker_t *wr = &(lb->lb_workers[i]);
            jk_worker_t *jw = wr->worker;
            ajp_worker_t *aw = (ajp_worker_t *)jw->worker_private;;

            jk_putv(s, "<tr><td>", wr->name, "</td><td>\n", NULL);

            if (!strcmp(attribute, JK_STATUS_ARG_LBM_ACTIVATION)) {

                jk_printf(s, "Active:&nbsp;<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"radio\"", i);
                jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_ACTIVE);
                if (wr->activation == JK_LB_ACTIVATION_ACTIVE)
                    jk_puts(s, " checked=\"checked\"");
                jk_puts(s, "/>&nbsp;|&nbsp;\n");
                jk_printf(s, "Disabled:&nbsp;<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"radio\"", i);
                jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_DISABLED);
                if (wr->activation == JK_LB_ACTIVATION_DISABLED)
                    jk_puts(s, " checked=\"checked\"");
                jk_puts(s, "/>&nbsp;|&nbsp;\n");
                jk_printf(s, "Stopped:&nbsp;<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"radio\"", i);
                jk_printf(s, " value=\"%d\"", JK_LB_ACTIVATION_STOPPED);
                if (wr->activation == JK_LB_ACTIVATION_STOPPED)
                    jk_puts(s, " checked=\"checked\"");
                jk_puts(s, "/>\n");

            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_FACTOR)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", wr->lb_factor);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_ROUTE)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_putv(s, "value=\"", wr->route, "\"/>\n", NULL);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_REDIRECT)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_putv(s, "value=\"", wr->redirect, "\"/>\n", NULL);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DOMAIN)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_putv(s, "value=\"", wr->domain, "\"/>\n", NULL);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DISTANCE)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", wr->distance);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CACHE_TO)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->cache_timeout);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_PING_TO)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->ping_timeout);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CONNECT_TO)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->connect_timeout);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_PREPOST_TO)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->prepost_timeout);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_REPLY_TO)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->reply_timeout);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_RETRIES)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->retries);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_RETRY_INT)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->retry_interval);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CPING_INT)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->conn_ping_interval);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_REC_OPTS)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->recovery_opts);
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_MAX_PK_SZ)) {
                jk_printf(s, "<input name=\"" JK_STATUS_ARG_MULT_VALUE_BASE "%d\" type=\"text\"", i);
                jk_printf(s, "value=\"%d\"/>\n", aw->max_packet_size);
            }

            jk_puts(s, "</td></tr>");
        }

        jk_puts(s, "</table>\n");
        jk_puts(s, "<br/><input type=\"submit\" value=\"Update Balancer\"/></form>\n");
    }
    JK_TRACE_EXIT(l);
}

static void commit_worker(jk_ws_service_t *s,
                          status_endpoint_t *p,
                          jk_worker_t *jw,
                          jk_logger_t *l)
{
    const char *name = NULL;
    lb_worker_t *lb = NULL;
    status_worker_t *w = p->worker;
    const char *arg;
    int sync_needed = JK_FALSE;
    int i;

    JK_TRACE_ENTER(l);
    if (jw->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)jw->worker_private;
        name = lb->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' committing changes for lb worker '%s'",
                   w->name, name);
    }
    else {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' worker type not implemented",
               w->name);
        JK_TRACE_EXIT(l);
        return;
    }

    if (!lb) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' lb structure is (null)",
               w->name);
        JK_TRACE_EXIT(l);
        return;
    }

    i = status_get_int(p, JK_STATUS_ARG_LB_RETRIES,
                       lb->retries, l);
    if (i != lb->retries && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "Status worker '%s' setting 'retries' for lb worker '%s' to '%i'",
               w->name, name, i);
        lb->retries = i;
        sync_needed = JK_TRUE;
    }
    i = status_get_int(p, JK_STATUS_ARG_LB_RETRY_INT,
                       lb->retry_interval, l);
    if (i != lb->retry_interval && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "Status worker '%s' setting 'retry_interval' for lb worker '%s' to '%i'",
               w->name, name, i);
        lb->retry_interval = i;
        sync_needed = JK_TRUE;
    }
    i = status_get_int(p, JK_STATUS_ARG_LB_RECOVER_TIME,
                       lb->recover_wait_time, l);
    if (i != lb->recover_wait_time && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "Status worker '%s' setting 'recover_time' for lb worker '%s' to '%i'",
               w->name, name, i);
        lb->recover_wait_time = i;
        sync_needed = JK_TRUE;
    }
    i = status_get_int(p, JK_STATUS_ARG_LB_ERROR_ESCALATION_TIME,
                       lb->error_escalation_time, l);
    if (i != lb->error_escalation_time && i > 0) {
        jk_log(l, JK_LOG_INFO,
               "Status worker '%s' setting 'error_escalation_time' for lb worker '%s' to '%i'",
               w->name, name, i);
        lb->error_escalation_time = i;
        sync_needed = JK_TRUE;
    }
    i = status_get_int(p, JK_STATUS_ARG_LB_MAX_REPLY_TIMEOUTS,
                       lb->max_reply_timeouts, l);
    if (i != lb->max_reply_timeouts && i >= 0) {
        jk_log(l, JK_LOG_INFO,
               "Status worker '%s' setting 'max_reply_timeouts' for lb worker '%s' to '%i'",
               w->name, name, i);
        lb->max_reply_timeouts = i;
        sync_needed = JK_TRUE;
    }
    i = status_get_bool(p, JK_STATUS_ARG_LB_STICKY, 0, l);
    if (i != lb->sticky_session) {
        jk_log(l, JK_LOG_INFO,
               "Status worker '%s' setting 'sticky_session' for lb worker '%s' to '%i'",
               w->name, name, i);
        lb->sticky_session = i;
        sync_needed = JK_TRUE;
    }
    i = status_get_bool(p, JK_STATUS_ARG_LB_STICKY_FORCE, 0, l);
    if (i != lb->sticky_session_force) {
        jk_log(l, JK_LOG_INFO,
               "Status worker '%s' setting 'sticky_session_force' for lb worker '%s' to '%i'",
               w->name, name, i);
        lb->sticky_session_force = i;
        sync_needed = JK_TRUE;
    }
    if (status_get_string(p, JK_STATUS_ARG_LB_METHOD, NULL, &arg, l) == JK_TRUE) {
        i = jk_lb_get_method_code(arg);
        if (i != lb->lbmethod && i >= 0 && i <= JK_LB_METHOD_MAX) {
            jk_log(l, JK_LOG_INFO,
                   "Status worker '%s' setting 'method' for lb worker '%s' to '%s'",
                   w->name, name, jk_lb_get_method(lb, l));
            lb->lbmethod = i;
            sync_needed = JK_TRUE;
        }
    }
    if (status_get_string(p, JK_STATUS_ARG_LB_LOCK, NULL, &arg, l) == JK_TRUE) {
        i = jk_lb_get_lock_code(arg);
        if (i != lb->lblock && i >= 0 && i <= JK_LB_LOCK_MAX) {
            jk_log(l, JK_LOG_INFO,
                   "Status worker '%s' setting 'lock' for lb worker '%s' to '%s'",
                   w->name, name, jk_lb_get_lock(lb, l));
            lb->lblock = i;
            sync_needed = JK_TRUE;
        }
    }
    if (sync_needed == JK_TRUE) {
        lb->sequence++;
        jk_lb_push(lb, JK_TRUE, l);
    }
}

static int set_int_if_changed(status_endpoint_t *p,
                              const char *name,
                              const char *att,
                              const char *arg,
                              int min,
                              int max,
                              int *param,
                              const char *lb_name,
                              jk_logger_t *l)
{
    int i;
    status_worker_t *w = p->worker;
    i = status_get_int(p, arg, *param, l);
    if (i != *param && i >= min && i <= max) {
        if (lb_name)
            jk_log(l, JK_LOG_INFO,
                   "Status worker '%s' setting '%s' for sub worker '%s' of lb worker '%s' to '%i'",
                   w->name, att, name, lb_name, i);
        else
            jk_log(l, JK_LOG_INFO,
                   "Status worker '%s' setting '%s' for ajp worker '%s' to '%i'",
                   w->name, att, name, i);
        *param = i;
        return JK_TRUE;
    }
    return JK_FALSE;
}

static int set_uint_if_changed(status_endpoint_t *p,
                               const char *name,
                               const char *att,
                               const char *arg,
                               unsigned int min,
                               unsigned int max,
                               unsigned int *param,
                               const char *lb_name,
                              jk_logger_t *l)
{
    unsigned i;
    status_worker_t *w = p->worker;
    i = (unsigned)status_get_int(p, arg, *param, l);
    if (i != *param && i >= min && i <= max) {
        if (lb_name)
            jk_log(l, JK_LOG_INFO,
                   "Status worker '%s' setting '%s' for sub worker '%s' of lb worker '%s' to '%u'",
                   w->name, att, name, lb_name, i);
        else
            jk_log(l, JK_LOG_INFO,
                   "Status worker '%s' setting '%s' for ajp worker '%s' to '%u'",
                   w->name, att, name, i);
        *param = i;
        return JK_TRUE;
    }
    return JK_FALSE;
}

static int commit_member(jk_ws_service_t *s,
                         status_endpoint_t *p,
                         lb_worker_t *lb,
                         lb_sub_worker_t *wr,
                         ajp_worker_t *aw,
                         int *side_effect,
                         jk_logger_t *l)
{
    const char *arg;
    const char *lb_name = NULL;
    status_worker_t *w = p->worker;
    int rc = JK_TRUE;
    int rv;
    int i;
    int old;
    int resolve = JK_FALSE;
    char host[JK_SHM_STR_SIZ+1];
    int  port = 0;

    JK_TRACE_ENTER(l);
    if (lb) {
        lb_name = lb->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' committing changes for sub worker '%s' of lb worker '%s'",
                   w->name, wr->name, lb_name);
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' committing changes for ajp worker '%s'",
                   w->name, aw->name);
    }

    if (lb) {
        if (status_get_string(p, JK_STATUS_ARG_LBM_ACTIVATION, NULL, &arg, l) == JK_TRUE) {
            i = jk_lb_get_activation_code(arg);
            if (i != wr->activation && i >= 0 && i <= JK_LB_ACTIVATION_MAX) {
                wr->activation = i;
                jk_log(l, JK_LOG_INFO,
                       "Status worker '%s' setting 'activation' for sub worker '%s' of lb worker '%s' to '%s'",
                       w->name, wr->name, lb_name, jk_lb_get_activation(wr, l));
                *side_effect |= JK_STATUS_NEEDS_RESET_LB_VALUES | JK_STATUS_NEEDS_PUSH;
            }
        }
        if (set_int_if_changed(p, wr->name, "lbfactor", JK_STATUS_ARG_LBM_FACTOR,
                               1, INT_MAX, &wr->lb_factor, lb_name, l))
            /* Recalculate the load multiplicators wrt. lb_factor */
            *side_effect |= JK_STATUS_NEEDS_UPDATE_MULT | JK_STATUS_NEEDS_PUSH;
        if ((rv = status_get_string(p, JK_STATUS_ARG_LBM_ROUTE,
                                    NULL, &arg, l)) == JK_TRUE) {
            if (strncmp(wr->route, arg, JK_SHM_STR_SIZ)) {
                jk_log(l, JK_LOG_INFO,
                       "Status worker '%s' setting 'route' for sub worker '%s' of lb worker '%s' to '%s'",
                       w->name, wr->name, lb_name, arg);
                strncpy(wr->route, arg, JK_SHM_STR_SIZ);
                *side_effect |= JK_STATUS_NEEDS_PUSH;
                if (!wr->domain[0]) {
                    char * id_domain = strchr(wr->route, '.');
                    if (id_domain) {
                        *id_domain = '\0';
                        strcpy(wr->domain, wr->route);
                        *id_domain = '.';
                    }
                }
            }
        }
        if ((rv = status_get_string(p, JK_STATUS_ARG_LBM_REDIRECT,
                                    NULL, &arg, l)) == JK_TRUE) {
            if (strncmp(wr->redirect, arg, JK_SHM_STR_SIZ)) {
                jk_log(l, JK_LOG_INFO,
                       "Status worker '%s' setting 'redirect' for sub worker '%s' of lb worker '%s' to '%s'",
                       w->name, wr->name, lb_name, arg);
                strncpy(wr->redirect, arg, JK_SHM_STR_SIZ);
                *side_effect |= JK_STATUS_NEEDS_PUSH;
            }
        }
        if ((rv = status_get_string(p, JK_STATUS_ARG_LBM_DOMAIN,
                                    NULL, &arg, l)) == JK_TRUE) {
            if (strncmp(wr->domain, arg, JK_SHM_STR_SIZ)) {
                jk_log(l, JK_LOG_INFO,
                       "Status worker '%s' setting 'domain' for sub worker '%s' of lb worker '%s' to '%s'",
                       w->name, wr->name, lb_name, arg);
                strncpy(wr->domain, arg, JK_SHM_STR_SIZ);
                *side_effect |= JK_STATUS_NEEDS_PUSH;
            }
        }
        if (set_int_if_changed(p, wr->name, "distance", JK_STATUS_ARG_LBM_DISTANCE,
                               0, INT_MAX, &wr->distance, lb_name, l))
            *side_effect |= JK_STATUS_NEEDS_PUSH;
    }
    old = aw->cache_timeout;
    if (set_int_if_changed(p, aw->name, "connection_pool_timeout", JK_STATUS_ARG_AJP_CACHE_TO,
                           0, INT_MAX, &aw->cache_timeout, lb_name, l)) {
        *side_effect |= JK_STATUS_NEEDS_PUSH;
        if (old == 0) {
            unsigned int i;
            for (i = 0; i < aw->ep_cache_sz; i++) {
                ajp_endpoint_t *ae = (ajp_endpoint_t *) aw->ep_cache[i];
                if (ae)
                    ae->last_access = time(NULL);
            }
        }
    }
    port = aw->port;
    if (set_int_if_changed(p, aw->name, "port", JK_STATUS_ARG_AJP_PORT,
                           0, INT_MAX, &port, lb_name, l)) {
        strncpy(host, aw->host, JK_SHM_STR_SIZ);
        resolve = JK_TRUE;
    }
    if ((rv = status_get_string(p, JK_STATUS_ARG_AJP_HOST_STR,
                                NULL, &arg, l)) == JK_TRUE) {
        if (strncmp(aw->host, arg, JK_SHM_STR_SIZ)) {
            jk_log(l, JK_LOG_INFO,
                    "Status worker '%s' setting 'host' for sub worker '%s' to '%s'",
                    w->name, aw->name, arg);
            strncpy(host, arg, JK_SHM_STR_SIZ);
            resolve = JK_TRUE;
        }
    }
    if (resolve == JK_TRUE) {
        struct sockaddr_in inet_addr;
        if (!jk_resolve(host, port, &inet_addr,
                        aw->worker.we->pool, l)) {
            const char *msg = "Update failed (at least partially): could not resolve address '%s:%d' for sub worker '%s'.";
            size_t size = strlen(msg) + strlen(host) + strlen(aw->name) + 10 + 1;
            p->msg = jk_pool_alloc(s->pool, size);
            snprintf(p->msg, size, msg, host, port, aw->name);
            jk_log(l, JK_LOG_ERROR,
                   "Status worker '%s' failed resolving address '%s:%d' for sub worker '%s'.",
                   w->name, host, port, aw->name);
            rc = JK_FALSE;
        }
        else {
            aw->port = port;
            strncpy(aw->host, host, JK_SHM_STR_SIZ);
            memcpy(&(aw->worker_inet_addr), &inet_addr, sizeof(inet_addr));
            *side_effect |= JK_STATUS_NEEDS_PUSH | JK_STATUS_NEEDS_ADDR_PUSH;
        }
    }
    if (set_int_if_changed(p, aw->name, "ping_timeout", JK_STATUS_ARG_AJP_PING_TO,
                           0, INT_MAX, &aw->ping_timeout, lb_name, l))
        *side_effect |= JK_STATUS_NEEDS_PUSH;
    if (set_int_if_changed(p, aw->name, "connect_timeout", JK_STATUS_ARG_AJP_CONNECT_TO,
                           0, INT_MAX, &aw->connect_timeout, lb_name, l))
        *side_effect |= JK_STATUS_NEEDS_PUSH;
    if (set_int_if_changed(p, aw->name, "prepost_timeout", JK_STATUS_ARG_AJP_PREPOST_TO,
                           0, INT_MAX, &aw->prepost_timeout, lb_name, l))
        *side_effect |= JK_STATUS_NEEDS_PUSH;
    if (set_int_if_changed(p, aw->name, "reply_timeout", JK_STATUS_ARG_AJP_REPLY_TO,
                           0, INT_MAX, &aw->reply_timeout, lb_name, l))
        *side_effect |= JK_STATUS_NEEDS_PUSH;
    if (set_int_if_changed(p, aw->name, "retries", JK_STATUS_ARG_AJP_RETRIES,
                           1, INT_MAX, &aw->retries, lb_name, l))
        *side_effect |= JK_STATUS_NEEDS_PUSH;
    if (set_int_if_changed(p, aw->name, "retry_interval", JK_STATUS_ARG_AJP_RETRY_INT,
                           1, INT_MAX, &aw->retry_interval, lb_name, l))
        *side_effect |= JK_STATUS_NEEDS_PUSH;
    if (set_int_if_changed(p, aw->name, "connection_ping_interval", JK_STATUS_ARG_AJP_CPING_INT,
                           1, INT_MAX, &aw->conn_ping_interval, lb_name, l))
        *side_effect |= JK_STATUS_NEEDS_PUSH;
    if (set_uint_if_changed(p, aw->name, "recovery_options", JK_STATUS_ARG_AJP_REC_OPTS,
                           0, INT_MAX, &aw->recovery_opts, lb_name, l))
        *side_effect |= JK_STATUS_NEEDS_PUSH;
    if (set_uint_if_changed(p, aw->name, "max_packet_size", JK_STATUS_ARG_AJP_MAX_PK_SZ,
                           8*1024, 64*1024, &aw->max_packet_size, lb_name, l)) {
        *side_effect |= JK_STATUS_NEEDS_PUSH;
        if (aw->max_packet_size > lb->max_packet_size) {
            lb->max_packet_size = aw->max_packet_size;
        }
    }
    return rc;
}

static void commit_all_members(jk_ws_service_t *s,
                               status_endpoint_t *p,
                               jk_worker_t *jw,
                               const char *attribute,
                               jk_logger_t *l)
{
    const char *arg;
    char vname[32];
    const char *name = NULL;
    lb_worker_t *lb = NULL;
    status_worker_t *w = p->worker;
    const char *aname;
    int i;
    int rc = 0;
    unsigned int j;

    JK_TRACE_ENTER(l);
    if (!attribute) {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' missing request parameter '%s'",
               w->name, JK_STATUS_ARG_ATTRIBUTE);
        JK_TRACE_EXIT(l);
        return;
    }
    else {
        if (!strcmp(attribute, JK_STATUS_ARG_LBM_ACTIVATION))
            aname=JK_STATUS_ARG_LBM_TEXT_ACTIVATION;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_FACTOR))
            aname=JK_STATUS_ARG_LBM_TEXT_FACTOR;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_ROUTE))
            aname=JK_STATUS_ARG_LBM_TEXT_ROUTE;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_REDIRECT))
            aname=JK_STATUS_ARG_LBM_TEXT_REDIRECT;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DOMAIN))
            aname=JK_STATUS_ARG_LBM_TEXT_DOMAIN;
        else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DISTANCE))
            aname=JK_STATUS_ARG_LBM_TEXT_DISTANCE;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CACHE_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_CACHE_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_PING_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_PING_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CONNECT_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_CONNECT_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_PREPOST_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_PREPOST_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_REPLY_TO))
            aname=JK_STATUS_ARG_AJP_TEXT_REPLY_TO;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_RETRIES))
            aname=JK_STATUS_ARG_AJP_TEXT_RETRIES;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_RETRY_INT))
            aname=JK_STATUS_ARG_AJP_TEXT_RETRY_INT;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CPING_INT))
            aname=JK_STATUS_ARG_AJP_TEXT_CPING_INT;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_REC_OPTS))
            aname=JK_STATUS_ARG_AJP_TEXT_REC_OPTS;
        else if (!strcmp(attribute, JK_STATUS_ARG_AJP_MAX_PK_SZ))
            aname=JK_STATUS_ARG_AJP_TEXT_MAX_PK_SZ;
        else {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' unknown attribute '%s'",
                   w->name, attribute);
            JK_TRACE_EXIT(l);
            return;
        }
    }
    if (jw->type == JK_LB_WORKER_TYPE) {
        lb = (lb_worker_t *)jw->worker_private;
        name = lb->name;
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' committing changes for attribute '%s' [%s] of all members of lb worker '%s'",
                   w->name, attribute, aname, name);
    }
    else {
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' worker type not implemented",
               w->name);
        JK_TRACE_EXIT(l);
        return;
    }

    if (lb) {
        for (j = 0; j < lb->num_of_workers; j++) {
            int sync_needed = JK_FALSE;
            lb_sub_worker_t *wr = &(lb->lb_workers[j]);
            jk_worker_t *jw = wr->worker;
            ajp_worker_t *aw = (ajp_worker_t *)jw->worker_private;;
            snprintf(vname, 32-1, "" JK_STATUS_ARG_MULT_VALUE_BASE "%d", j);

            if (!strcmp(attribute, JK_STATUS_ARG_LBM_FACTOR)) {
                if (set_int_if_changed(p, wr->name, "lbfactor", vname,
                                       1, INT_MAX, &wr->lb_factor, name, l)) {
                    rc = 2;
                    sync_needed = JK_TRUE;
                }
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DISTANCE)) {
                if (set_int_if_changed(p, wr->name, "distance", vname,
                                       0, INT_MAX, &wr->distance, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CACHE_TO)) {
                int old = aw->cache_timeout;
                if (set_int_if_changed(p, aw->name, "connection_pool_timeout", vname,
                                       0, INT_MAX, &aw->cache_timeout, name, l)) {
                    sync_needed = JK_TRUE;
                    if (old == 0) {
                        unsigned int i;
                        for (i = 0; i < aw->ep_cache_sz; i++) {
                            ajp_endpoint_t *ae = (ajp_endpoint_t *) aw->ep_cache[i];
                            if (ae)
                                ae->last_access = time(NULL);
                        }
                    }
                }
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_PING_TO)) {
                if (set_int_if_changed(p, aw->name, "ping_timeout", vname,
                                       0, INT_MAX, &aw->ping_timeout, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CONNECT_TO)) {
                if (set_int_if_changed(p, aw->name, "connect_timeout", vname,
                                       0, INT_MAX, &aw->connect_timeout, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_PREPOST_TO)) {
                if (set_int_if_changed(p, aw->name, "prepost_timeout", vname,
                                       0, INT_MAX, &aw->prepost_timeout, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_REPLY_TO)) {
                if (set_int_if_changed(p, aw->name, "reply_timeout", vname,
                                       0, INT_MAX, &aw->reply_timeout, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_RETRIES)) {
                if (set_int_if_changed(p, aw->name, "retries", vname,
                                       1, INT_MAX, &aw->retries, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_RETRY_INT)) {
                if (set_int_if_changed(p, aw->name, "retry_interval", vname,
                                       1, INT_MAX, &aw->retry_interval, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_CPING_INT)) {
                if (set_int_if_changed(p, aw->name, "connection_ping_interval", vname,
                                       1, INT_MAX, &aw->conn_ping_interval, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_REC_OPTS)) {
                if (set_uint_if_changed(p, aw->name, "recovery_options", vname,
                                       0, INT_MAX, &aw->recovery_opts, name, l))
                    sync_needed = JK_TRUE;
            }
            else if (!strcmp(attribute, JK_STATUS_ARG_AJP_MAX_PK_SZ)) {
                if (set_uint_if_changed(p, aw->name, "max_packet_size", vname,
                                       8*1024, 64*1024, &aw->max_packet_size, name, l)) {
                    sync_needed = JK_TRUE;
                    if (aw->max_packet_size > lb->max_packet_size) {
                        lb->max_packet_size = aw->max_packet_size;
                    }
                }
            }
            else {
                int rv = status_get_string(p, vname, NULL, &arg, l);
                if (!strcmp(attribute, JK_STATUS_ARG_LBM_ACTIVATION)) {
                    if (rv == JK_TRUE) {
                        i = jk_lb_get_activation_code(arg);
                        if (i != wr->activation && i >= 0 && i <= JK_LB_ACTIVATION_MAX) {
                            jk_log(l, JK_LOG_INFO,
                                   "Status worker '%s' setting 'activation' for sub worker '%s' of lb worker '%s' to '%s'",
                                   w->name, wr->name, name, jk_lb_get_activation(wr, l));
                            wr->activation = i;
                            rc = 1;
                            sync_needed = JK_TRUE;
                        }
                    }
                }
                else if (!strcmp(attribute, JK_STATUS_ARG_LBM_ROUTE)) {
                    if (rv == JK_TRUE) {
                        if (strncmp(wr->route, arg, JK_SHM_STR_SIZ)) {
                            jk_log(l, JK_LOG_INFO,
                                   "Status worker '%s' setting 'route' for sub worker '%s' of lb worker '%s' to '%s'",
                                   w->name, wr->name, name, arg);
                            strncpy(wr->route, arg, JK_SHM_STR_SIZ);
                            sync_needed = JK_TRUE;
                            if (!wr->domain[0]) {
                                char * id_domain = strchr(wr->route, '.');
                                if (id_domain) {
                                    *id_domain = '\0';
                                    strcpy(wr->domain, wr->route);
                                    *id_domain = '.';
                                }
                            }
                        }
                    }
                }
                else if (!strcmp(attribute, JK_STATUS_ARG_LBM_REDIRECT)) {
                    if (rv == JK_TRUE) {
                        if (strncmp(wr->redirect, arg, JK_SHM_STR_SIZ)) {
                            jk_log(l, JK_LOG_INFO,
                                   "Status worker '%s' setting 'redirect' for sub worker '%s' of lb worker '%s' to '%s'",
                                   w->name, wr->name, name, arg);
                            strncpy(wr->redirect, arg, JK_SHM_STR_SIZ);
                            sync_needed = JK_TRUE;
                        }
                    }
                }
                else if (!strcmp(attribute, JK_STATUS_ARG_LBM_DOMAIN)) {
                    if (rv == JK_TRUE) {
                        if (strncmp(wr->domain, arg, JK_SHM_STR_SIZ)) {
                            jk_log(l, JK_LOG_INFO,
                                   "Status worker '%s' setting 'domain' for sub worker '%s' of lb worker '%s' to '%s'",
                                   w->name, wr->name, name, arg);
                            strncpy(wr->domain, arg, JK_SHM_STR_SIZ);
                            sync_needed = JK_TRUE;
                        }
                    }
                }
            }
            if (sync_needed == JK_TRUE) {
                wr->sequence++;
                if (!rc)
                    rc = 3;
            }
        }
        if (rc == 1)
            reset_lb_values(lb, l);
        else if (rc == 2)
            /* Recalculate the load multiplicators wrt. lb_factor */
            update_mult(lb, l);
        if (rc) {
            lb->sequence++;
            jk_lb_push(lb, JK_TRUE, l);
        }
    }
    JK_TRACE_EXIT(l);
}

static void display_legend(jk_ws_service_t *s,
                           status_endpoint_t *p,
                           jk_logger_t *l)
{

    int mime;
    const char *arg;
    unsigned int hide_legend;

    JK_TRACE_ENTER(l);
    status_get_string(p, JK_STATUS_ARG_MIME, NULL, &arg, l);
    mime = status_mime_int(arg);
    if (mime != JK_STATUS_MIME_HTML) {
        JK_TRACE_EXIT(l);
        return;
    }
    hide_legend = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                               JK_STATUS_ARG_OPTION_NO_LEGEND;
    if (hide_legend) {
        jk_puts(s, "<p>\n");
        status_write_uri(s, p, "Show Legend", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                         NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_LEGEND, NULL, l);
        jk_puts(s, "</p>\n");
    }
    else {
        jk_puts(s, "<h2>Legend [");
        status_write_uri(s, p, "Hide", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                         NULL, NULL, JK_STATUS_ARG_OPTION_NO_LEGEND, 0, NULL, l);
        jk_puts(s, "]</h2>\n");

        jk_puts(s, "<table>\n"
            "<tbody valign=\"baseline\">\n"
            "<tr><th>Name</th><td>Worker name</td></tr>\n"
            "<tr><th>Type</th><td>Worker type</td></tr>\n"
            "<tr><th>Route</th><td>Worker route</td></tr>\n"
            "<tr><th>Act</th><td>Worker activation configuration<br/>\n"
            "ACT=Active, DIS=Disabled, STP=Stopped</td></tr>\n"
            "<tr><th>State</th><td>Worker error status<br/>\n"
            "OK=OK, ERR=Error with substates<br/>\n"
            "IDLE=No requests handled, BUSY=All connections busy,<br/>\n"
            "REC=Recovering, PRB=Probing, FRC=Forced Recovery</td></tr>\n"
            "<tr><th>D</th><td>Worker distance</td></tr>\n"
            "<tr><th>F</th><td>Load Balancer factor</td></tr>\n"
            "<tr><th>M</th><td>Load Balancer multiplicity</td></tr>\n"
            "<tr><th>V</th><td>Load Balancer value</td></tr>\n"
            "<tr><th>Acc</th><td>Number of requests</td></tr>\n"
            "<tr><th>Err</th><td>Number of failed requests</td></tr>\n"
            "<tr><th>CE</th><td>Number of client errors</td></tr>\n"
            "<tr><th>RE</th><td>Number of reply timeouts (decayed)</td></tr>\n"
            "<tr><th>Wr</th><td>Number of bytes transferred</td></tr>\n"
            "<tr><th>Rd</th><td>Number of bytes read</td></tr>\n"
            "<tr><th>Busy</th><td>Current number of busy connections</td></tr>\n"
            "<tr><th>Max</th><td>Maximum number of busy connections</td></tr>\n"
            "<tr><th>RR</th><td>Route redirect</td></tr>\n"
            "<tr><th>Cd</th><td>Cluster domain</td></tr>\n"
            "<tr><th>Rs</th><td>Recovery scheduled in app. min/max seconds</td></tr>\n"
            "<tr><th>LR</th><td>Seconds since last reset of statistics counters</td></tr>\n"
            "<tr><th>LE</th><td>Timestamp of the last error</td></tr>\n"
            "</tbody>\n"
            "</table>\n");
    }

    JK_TRACE_EXIT(l);
}

static int check_worker(jk_ws_service_t *s,
                        status_endpoint_t *p,
                        jk_uint32_t allow_wildchars,
                        jk_logger_t *l)
{
    const char *worker;
    const char *sub_worker;
    status_worker_t *w = p->worker;
    jk_worker_t *jw = NULL;
    lb_sub_worker_t *wr = NULL;

    JK_TRACE_ENTER(l);
    fetch_worker_and_sub_worker(p, "checking", &worker, &sub_worker, l);
    if (search_worker(s, p, &jw, worker, l) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    if (sub_worker && sub_worker[0]) {
        unsigned int idx = 0;
        unsigned int *wi = NULL;
        if (strchr(sub_worker, '*') || strchr(sub_worker, '?')) {
            /* We have a wildchar matching rule */
            if (!allow_wildchars) {
                jk_log(l, JK_LOG_ERROR,
                       "Status worker '%s' wildcards in sub worker '%s' of worker '%s' not allowed for this command",
                       w->name, sub_worker, worker ? worker : "(null)");
                p->msg = "wildcard not allowed in sub worker for this command";
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
            else {
                wi = &idx;
            }
        }
        if (search_sub_worker(s, p, jw, worker, &wr, sub_worker,
                              wi, l) == JK_FALSE) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static void count_workers(jk_ws_service_t *s,
                          status_endpoint_t *p,
                          int *lb_cnt, int *ajp_cnt,
                          jk_logger_t *l)
{
    unsigned int i;
    jk_worker_t *jw = NULL;
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    *lb_cnt = 0;
    *ajp_cnt = 0;
    for (i = 0; i < w->we->num_of_workers; i++) {
        jw = wc_get_worker_for_name(w->we->worker_list[i], l);
        if (!jw) {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' could not find worker '%s'",
                   w->name, w->we->worker_list[i]);
            continue;
        }
        if (jw->type == JK_LB_WORKER_TYPE) {
            (*lb_cnt)++;
        }
        else if (jw->type == JK_AJP13_WORKER_TYPE ||
                 jw->type == JK_AJP14_WORKER_TYPE) {
            (*ajp_cnt)++;
        }
    }
    JK_TRACE_EXIT(l);
}

static void list_workers_type(jk_ws_service_t *s,
                              status_endpoint_t *p,
                              int list_lb, int count,
                              jk_logger_t *l)
{

    const char *arg;
    unsigned int i;
    int mime;
    unsigned int hide;
    jk_worker_t *jw = NULL;
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);

    status_get_string(p, JK_STATUS_ARG_MIME, NULL, &arg, l);
    mime = status_mime_int(arg);
    if (list_lb) {
        hide = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                              JK_STATUS_ARG_OPTION_NO_LB;
        if (hide) {
            if (mime == JK_STATUS_MIME_HTML) {
                jk_puts(s, "<p>\n");
                status_write_uri(s, p, "Show Load Balancing Workers", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_LB, NULL, l);
                jk_puts(s, "</p>\n");
            }
        }
        else {
            if (mime == JK_STATUS_MIME_XML) {
                jk_print_xml_start_elt(s, w, 0, 0, "balancers");
                jk_print_xml_att_int(s, 2, "count", count);
                jk_print_xml_stop_elt(s, 0, 0);
            }
            else if (mime == JK_STATUS_MIME_TXT) {
                jk_printf(s, "Balancer Workers: count=%d\n", count);
            }
            else if (mime == JK_STATUS_MIME_PROP) {
                jk_print_prop_att_int(s, w, NULL, "lb_count", count);
            }
            else {
                jk_printf(s, "<hr/><h2>Listing Load Balancing Worker%s (%d Worker%s) [",
                          count>1 ? "s" : "", count, count>1 ? "s" : "");
                status_write_uri(s, p, "Hide", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, JK_STATUS_ARG_OPTION_NO_LB, 0, NULL, l);
                jk_puts(s, "]</h2>\n");
            }
        }
    }
    else {
        hide = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                              JK_STATUS_ARG_OPTION_NO_AJP;
        if (hide) {
            if (mime == JK_STATUS_MIME_HTML) {
                jk_puts(s, "<p>\n");
                status_write_uri(s, p, "Show AJP Workers", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, 0, JK_STATUS_ARG_OPTION_NO_AJP, NULL, l);
                jk_puts(s, "</p>\n");
            }
        }
        else {
            if (mime == JK_STATUS_MIME_XML) {
                jk_print_xml_start_elt(s, w, 0, 0, "ajp_workers");
                jk_print_xml_att_int(s, 2, "count", count);
                jk_print_xml_stop_elt(s, 0, 0);
            }
            else if (mime == JK_STATUS_MIME_TXT) {
                jk_printf(s, "AJP Workers: count=%d\n", count);
            }
            else if (mime == JK_STATUS_MIME_PROP) {
                jk_print_prop_att_int(s, w, NULL, "ajp_count", count);
            }
            else {
                jk_printf(s, "<hr/><h2>Listing AJP Worker%s (%d Worker%s) [",
                          count>1 ? "s" : "", count, count>1 ? "s" : "");
                status_write_uri(s, p, "Hide", JK_STATUS_CMD_UNKNOWN, JK_STATUS_MIME_UNKNOWN,
                                 NULL, NULL, JK_STATUS_ARG_OPTION_NO_AJP, 0, NULL, l);
                jk_puts(s, "]</h2>\n");
            }
        }
    }

    if (hide) {
        JK_TRACE_EXIT(l);
        return;
    }

    for (i = 0; i < w->we->num_of_workers; i++) {
        jw = wc_get_worker_for_name(w->we->worker_list[i], l);
        if (!jw) {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' could not find worker '%s'",
                   w->name, w->we->worker_list[i]);
            continue;
        }
        if ((list_lb && jw->type == JK_LB_WORKER_TYPE) ||
            (!list_lb && jw->type != JK_LB_WORKER_TYPE)) {
            display_worker(s, p, jw, NULL, l);
        }
    }

    if (list_lb) {
        if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_close_elt(s, w, 0, "balancers");
        }
        else if (mime == JK_STATUS_MIME_TXT) {
        }
        else if (mime == JK_STATUS_MIME_PROP) {
        }
        else if (mime == JK_STATUS_MIME_HTML) {
        }
    }
    else {
        if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_close_elt(s, w, 0, "ajp_workers");
        }
        else if (mime == JK_STATUS_MIME_TXT) {
        }
        else if (mime == JK_STATUS_MIME_PROP) {
        }
        else if (mime == JK_STATUS_MIME_HTML) {
        }
    }

    JK_TRACE_EXIT(l);
}

static int list_workers(jk_ws_service_t *s,
                        status_endpoint_t *p,
                        jk_logger_t *l)
{
    int lb_cnt = 0;
    int ajp_cnt = 0;

    JK_TRACE_ENTER(l);
    count_workers(s, p, &lb_cnt, &ajp_cnt, l);

    if (lb_cnt) {
        list_workers_type(s, p, 1, lb_cnt, l);
    }

    if (ajp_cnt) {
        list_workers_type(s, p, 0, ajp_cnt, l);
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int show_worker(jk_ws_service_t *s,
                       status_endpoint_t *p,
                       jk_logger_t *l)
{
    const char *worker;
    const char *sub_worker;
    jk_worker_t *jw = NULL;
    lb_sub_worker_t *wr = NULL;

    JK_TRACE_ENTER(l);
    fetch_worker_and_sub_worker(p, "showing", &worker, &sub_worker, l);
    if (search_worker(s, p, &jw, worker, l) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }
    if (sub_worker && sub_worker[0]) {
        if (search_sub_worker(s, p, jw, worker, &wr, sub_worker,
                             NULL, l) == JK_FALSE) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }
    }
    display_worker(s, p, jw, wr, l);

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int edit_worker(jk_ws_service_t *s,
                       status_endpoint_t *p,
                       jk_logger_t *l)
{
    const char *worker;
    const char *sub_worker;
    status_worker_t *w = p->worker;
    jk_worker_t *jw = NULL;
    lb_worker_t *lb = NULL;
    lb_sub_worker_t *wr = NULL;
    ajp_worker_t *aw = NULL;

    JK_TRACE_ENTER(l);
    fetch_worker_and_sub_worker(p, "editing", &worker, &sub_worker, l);
    if (search_worker(s, p, &jw, worker, l) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    if (jw->type == JK_LB_WORKER_TYPE) {
        if (check_valid_lb(s, p, jw, worker, &lb, 0, l) == JK_FALSE) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        if (lb->sequence != lb->s->h.sequence)
            jk_lb_pull(lb, JK_FALSE, l);
        if (!sub_worker || !sub_worker[0]) {
            const char *arg;
            if (status_get_string(p, JK_STATUS_ARG_ATTRIBUTE,
                                  NULL, &arg, l) == JK_TRUE) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Status worker '%s' %s lb worker '%s' with all sub workers",
                           w->name, "editing", lb->name);
                form_all_members(s, p, jw, arg, l);
            }
            else {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Status worker '%s' %s lb worker '%s'",
                           w->name, "editing", lb->name);
                form_worker(s, p, jw, l);
            }
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
        else {
            if (search_sub_worker(s, p, jw, worker, &wr, sub_worker,
                                 NULL, l) == JK_FALSE) {
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s lb worker '%s' sub worker '%s'",
                       w->name, "editing", lb->name, wr->name);
            aw = (ajp_worker_t *)wr->worker->worker_private;
            form_member(s, p, wr, aw, worker, l);
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
    }
    else if (jw->type == JK_AJP13_WORKER_TYPE ||
             jw->type == JK_AJP14_WORKER_TYPE) {
        ajp_worker_t *aw = (ajp_worker_t *)jw->worker_private;
        if (aw) {
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s ajp worker '%s'",
                       w->name, "editing", aw->name);
            if (aw->sequence != aw->s->h.sequence)
                jk_ajp_pull(aw, JK_FALSE, l);
            form_member(s, p, NULL, aw, worker, l);
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
        else {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' aw worker is (null)",
                   w->name);
        }
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' worker type not implemented",
                   w->name);
    }
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int update_worker(jk_ws_service_t *s,
                         status_endpoint_t *p,
                         jk_logger_t *l)
{
    const char *worker;
    const char *sub_worker;
    status_worker_t *w = p->worker;
    jk_worker_t *jw = NULL;
    lb_worker_t *lb = NULL;
    lb_sub_worker_t *wr = NULL;
    ajp_worker_t *aw = NULL;
    int rc = JK_TRUE;
    int rv;

    JK_TRACE_ENTER(l);
    fetch_worker_and_sub_worker(p, "updating", &worker, &sub_worker, l);
    if (search_worker(s, p, &jw, worker, l) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    if (jw->type == JK_LB_WORKER_TYPE) {
        if (check_valid_lb(s, p, jw, worker, &lb, 0, l) == JK_FALSE) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        if (lb->sequence != lb->s->h.sequence)
            jk_lb_pull(lb, JK_TRUE, l);
        if (!sub_worker || !sub_worker[0]) {
            const char *arg;
            if (status_get_string(p, JK_STATUS_ARG_ATTRIBUTE,
                                  NULL, &arg, l) == JK_TRUE) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Status worker '%s' %s lb worker '%s' with all sub workers",
                           w->name, "updating", lb->name);
                commit_all_members(s, p, jw, arg, l);
            }
            else {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                           "Status worker '%s' %s lb worker '%s'",
                           w->name, "updating", lb->name);
                commit_worker(s, p, jw, l);
            }
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
        else {
            unsigned int idx = 0;
            unsigned int *wi = NULL;
            int is_wildchar = JK_FALSE;

            if (strchr(sub_worker, '*') || strchr(sub_worker, '?')) {
                /* We have a wildchar matching rule */
                wi = &idx;
                is_wildchar = JK_TRUE;
            }
            for (;;) {
                if (search_sub_worker(s, p, jw, worker, &wr, sub_worker,
                                      wi, l) == JK_FALSE) {
                    if (!idx) {
                        JK_TRACE_EXIT(l);
                        return JK_FALSE;
                    }
                    else {
                        /* We have found at least one match previously */
                        p->msg = "OK";
                        break;
                    }
                }
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s lb worker '%s' sub worker '%s'",
                       w->name, "updating", lb->name, wr->name);
                aw = (ajp_worker_t *)wr->worker->worker_private;
                rv = 0;
                rc = commit_member(s, p, lb, wr, aw, &rv, l);
                if (rv & JK_STATUS_NEEDS_ADDR_PUSH) {
                    aw->addr_sequence++;
                }
                if (rv & (JK_STATUS_NEEDS_PUSH | JK_STATUS_NEEDS_ADDR_PUSH)) {
                    wr->sequence++;
                    lb->sequence++;
                    jk_lb_push(lb, JK_TRUE, l);
                }
                if (rv & JK_STATUS_NEEDS_RESET_LB_VALUES)
                    reset_lb_values(lb, l);
                if (rv & JK_STATUS_NEEDS_UPDATE_MULT)
                    /* Recalculate the load multiplicators wrt. lb_factor */
                    update_mult(lb, l);
                if (rc == JK_FALSE) {
                    jk_log(l, JK_LOG_ERROR,
                           "Status worker '%s' failed updating sub worker '%s' (at least partially).%s",
                           w->name, aw->name, (is_wildchar == JK_TRUE) ? " Aborting further wildcard updates." : "");
                    if (!strncmp("OK", p->msg, 3)) {
                        const char *msg = "Update failed (at least partially) for sub worker '%s'";
                        size_t size = strlen(msg) + strlen(aw->name) + 1;
                        p->msg = jk_pool_alloc(s->pool, size);
                        snprintf(p->msg, size, msg, aw->name);
                    }
                    if (is_wildchar == JK_TRUE) {
                        const char *msg = " Aborting further wildcard updates.";
                        size_t size = strlen(msg) + strlen(p->msg) + 1;
                        p->msg = jk_pool_realloc(s->pool, size, p->msg, strlen(p->msg) + 1);
                        strcat(p->msg, msg);
                    }
                    break;
                }
                if (!wi)
                    break;
            }
            JK_TRACE_EXIT(l);
            return rc;
        }
    }
    else if (jw->type == JK_AJP13_WORKER_TYPE ||
             jw->type == JK_AJP14_WORKER_TYPE) {
        ajp_worker_t *aw = (ajp_worker_t *)jw->worker_private;
        if (aw) {
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s ajp worker '%s'",
                       w->name, "updating", aw->name);
            if (aw->sequence != aw->s->h.sequence)
                jk_ajp_pull(aw, JK_TRUE, l);
            rv = 0;
            rc = commit_member(s, p, NULL, NULL, aw, &rv, l);
            if (rv & JK_STATUS_NEEDS_ADDR_PUSH) {
                aw->addr_sequence++;
            }
            if (rv & (JK_STATUS_NEEDS_PUSH | JK_STATUS_NEEDS_ADDR_PUSH)) {
                aw->sequence++;
                jk_ajp_push(aw, JK_TRUE, l);
            }
            if (rc == JK_FALSE) {
                jk_log(l, JK_LOG_ERROR,
                       "Status worker '%s' failed updating worker '%s' (at least partially).",
                       w->name, aw->name);
                if (!strncmp("OK", p->msg, 3)) {
                    const char *msg = "Update failed (at least partially) for worker '%s'";
                    size_t size = strlen(msg) + strlen(aw->name) + 1;
                    p->msg = jk_pool_alloc(s->pool, size);
                    snprintf(p->msg, size, msg, aw->name);
                }
            }
            JK_TRACE_EXIT(l);
            return rc;
        }
        else {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' aw worker is (null)",
                   w->name);
        }
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' worker type not implemented",
                   w->name);
    }
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int reset_worker(jk_ws_service_t *s,
                        status_endpoint_t *p,
                        jk_logger_t *l)
{
    unsigned int i;
    const char *worker;
    const char *sub_worker;
    status_worker_t *w = p->worker;
    jk_worker_t *jw = NULL;
    lb_worker_t *lb = NULL;
    lb_sub_worker_t *wr = NULL;
    ajp_worker_t *aw = NULL;
    time_t now = 0;

    JK_TRACE_ENTER(l);
    fetch_worker_and_sub_worker(p, "resetting", &worker, &sub_worker, l);
    if (search_worker(s, p, &jw, worker, l) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    now = time(NULL);
    if (jw->type == JK_LB_WORKER_TYPE) {
        if (check_valid_lb(s, p, jw, worker, &lb, 0, l) == JK_FALSE) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        if (!sub_worker || !sub_worker[0]) {
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s lb worker '%s' with all sub workers",
                       w->name, "resetting", lb->name);
            lb->s->max_busy = 0;
            lb->s->last_reset = now;
            for (i = 0; i < lb->num_of_workers; i++) {
                wr = &(lb->lb_workers[i]);
                aw = (ajp_worker_t *)wr->worker->worker_private;
                wr->s->state            = JK_LB_STATE_IDLE;
                wr->s->elected_snapshot = 0;
                wr->s->error_time       = 0;
                wr->s->errors           = 0;
                wr->s->lb_value         = 0;
                aw->s->used             = 0;
                aw->s->client_errors    = 0;
                aw->s->reply_timeouts   = 0;
                aw->s->transferred      = 0;
                aw->s->readed           = 0;
                aw->s->max_busy         = 0;
                aw->s->last_reset       = now;
            }
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
        else {
            if (search_sub_worker(s, p, jw, worker, &wr, sub_worker,
                                 NULL, l) == JK_FALSE) {
                JK_TRACE_EXIT(l);
                return JK_FALSE;
            }
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s lb worker '%s' sub worker '%s'",
                       w->name, "resetting", lb->name, wr->name);
            aw = (ajp_worker_t *)wr->worker->worker_private;
            wr->s->state            = JK_LB_STATE_IDLE;
            wr->s->elected_snapshot = 0;
            wr->s->error_time       = 0;
            wr->s->errors           = 0;
            wr->s->lb_value         = 0;
            aw->s->used             = 0;
            aw->s->client_errors    = 0;
            aw->s->reply_timeouts   = 0;
            aw->s->transferred      = 0;
            aw->s->readed           = 0;
            aw->s->max_busy         = 0;
            aw->s->last_reset       = now;
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
    }
    else if (jw->type == JK_AJP13_WORKER_TYPE ||
             jw->type == JK_AJP14_WORKER_TYPE) {
        ajp_worker_t *aw = (ajp_worker_t *)jw->worker_private;
        if (aw) {
            if (JK_IS_DEBUG_LEVEL(l))
                jk_log(l, JK_LOG_DEBUG,
                       "Status worker '%s' %s ajp worker '%s'",
                       w->name, "resetting", aw->name);
            aw->s->errors           = 0;
            aw->s->used             = 0;
            aw->s->client_errors    = 0;
            aw->s->reply_timeouts   = 0;
            aw->s->transferred      = 0;
            aw->s->readed           = 0;
            aw->s->max_busy         = 0;
            aw->s->last_reset       = now;
            JK_TRACE_EXIT(l);
            return JK_TRUE;
        }
        else {
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' aw worker is (null)",
                   w->name);
        }
    }
    else {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' worker type not implemented",
                   w->name);
    }
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int recover_worker(jk_ws_service_t *s,
                          status_endpoint_t *p,
                          jk_logger_t *l)
{
    const char *worker;
    const char *sub_worker;
    jk_worker_t *jw = NULL;
    lb_sub_worker_t *wr = NULL;
    ajp_worker_t *aw = NULL;
    status_worker_t *w = p->worker;

    JK_TRACE_ENTER(l);
    fetch_worker_and_sub_worker(p, "recovering", &worker, &sub_worker, l);
    if (search_worker(s, p, &jw, worker, l) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    if (search_sub_worker(s, p, jw, worker, &wr, sub_worker,
                         NULL, l) == JK_FALSE) {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    aw = (ajp_worker_t *)wr->worker->worker_private;
    if (wr->s->state == JK_LB_STATE_ERROR) {
        lb_worker_t *lb = NULL;

        /* We need an lb to correct the lb_value */
        if (check_valid_lb(s, p, jw, worker, &lb, 0, l) == JK_FALSE) {
            JK_TRACE_EXIT(l);
            return JK_FALSE;
        }

        if (lb->lbmethod != JK_LB_METHOD_BUSYNESS) {
            unsigned int i;
            jk_uint64_t curmax = 0;

            for (i = 0; i < lb->num_of_workers; i++) {
                if (lb->lb_workers[i].s->lb_value > curmax) {
                    curmax = lb->lb_workers[i].s->lb_value;
                }
            }
            wr->s->lb_value = curmax;
        }

        aw->s->reply_timeouts = 0;
        wr->s->state = JK_LB_STATE_RECOVER;
        jk_log(l, JK_LOG_INFO,
               "Status worker '%s' marked worker '%s' sub worker '%s' for recovery",
               w->name, worker ? worker : "(null)", sub_worker ? sub_worker : "(null)");
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    jk_log(l, JK_LOG_WARNING,
           "Status worker '%s' could not mark worker '%s' sub worker '%s' for recovery - state %s is not an error state",
           w->name, worker ? worker : "(null)", sub_worker ? sub_worker : "(null)",
           jk_lb_get_state(wr, l));
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int dump_config(jk_ws_service_t *s,
                       status_endpoint_t *p,
                       int mime, jk_logger_t *l)
{
    status_worker_t *w = p->worker;
    jk_worker_env_t *we = w->we;
    jk_map_t *init_data = we->init_data;

    JK_TRACE_ENTER(l);

    if (init_data) {
        int l = jk_map_size(init_data);
        int i;
        if (mime == JK_STATUS_MIME_HTML) {
            jk_puts(s, "<hr/><h2>Configuration Data</h2><hr/>\n");
            jk_puts(s, "This dump does not include any changes applied by the status worker\n");
            jk_puts(s, "to the configuration after the initial startup\n");
            jk_puts(s, "<PRE>\n");
        }
        else if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_start_elt(s, w, 2, 0, "configuration");
        }
        else if (mime == JK_STATUS_MIME_TXT) {
            jk_puts(s, "Configuration:\n");
        }
        for (i=0;i<l;i++) {
            const char *name = jk_map_name_at(init_data, i);
            if (name) {
                const char *value = jk_map_value_at(init_data, i);
                if (!value)
                    value = "(null)";
                if (mime == JK_STATUS_MIME_HTML ||
                    mime == JK_STATUS_MIME_PROP ||
                    mime == JK_STATUS_MIME_TXT) {
                    jk_putv(s, name, "=", value, "\n", NULL);
                }
                else if (mime == JK_STATUS_MIME_XML) {
                     jk_print_xml_att_string(s, 4, name, value);
                }
            }
        }
        if (mime == JK_STATUS_MIME_HTML) {
            jk_puts(s, "</PRE>\n");
        }
        else if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_stop_elt(s, 2, 1);
        }
    }
    else {
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

/*
 * Return values of service() method for status worker:
 * return value  is_error              reason
 * JK_FALSE      JK_HTTP_SERVER_ERROR  Invalid parameters (null values)
 * JK_TRUE       JK_HTTP_OK            All other cases
 */
static int JK_METHOD service(jk_endpoint_t *e,
                             jk_ws_service_t *s,
                             jk_logger_t *l, int *is_error)
{
    int cmd;
    jk_uint32_t cmd_props;
    int mime;
    int refresh;
    int read_only = 0;
    const char *arg;
    char *err = NULL;
    status_endpoint_t *p;
    status_worker_t *w;
    int denied = 0;

    JK_TRACE_ENTER(l);

    if (!e || !e->endpoint_private || !s || !is_error) {
        JK_LOG_NULL_PARAMS(l);
        if (is_error)
            *is_error = JK_HTTP_SERVER_ERROR;
        JK_TRACE_EXIT(l);
        return JK_FALSE;
    }

    p = e->endpoint_private;
    w = p->worker;

    /* Set returned error to OK */
    *is_error = JK_HTTP_OK;

    if (w->num_of_users) {
        if (s->remote_user) {
            unsigned int i;
            denied = 1;
            for (i = 0; i < w->num_of_users; i++) {
                if (w->user_case_insensitive) {
                    if (!strcasecmp(s->remote_user, w->user_names[i])) {
                        denied = 0;
                        break;
                    }
                }
                else {
                    if (!strcmp(s->remote_user, w->user_names[i])) {
                        denied = 0;
                        break;
                    }
                }
            }
        }
        else {
            denied = 2;
        }
    }

    /* Step 1: Process GET params and update configuration */
    if (status_parse_uri(s, p, l) != JK_TRUE) {
        err = "Error during parsing of URI";
    }
    status_get_string(p, JK_STATUS_ARG_CMD, NULL, &arg, l);
    cmd = status_cmd_int(arg);
    cmd_props = status_cmd_props(cmd);
    status_get_string(p, JK_STATUS_ARG_MIME, NULL, &arg, l);
    mime = status_mime_int(arg);
    refresh = status_get_int(p, JK_STATUS_ARG_REFRESH, 0, l);
    if (w->read_only) {
        read_only = 1;
    }
    else {
        read_only = status_get_int(p, JK_STATUS_ARG_OPTIONS, 0, l) &
                    JK_STATUS_ARG_OPTION_READ_ONLY;
    }

    if (mime == JK_STATUS_MIME_HTML) {
        s->start_response(s, 200, "OK", headers_names, headers_vhtml, 3);
        jk_puts(s, JK_STATUS_HEAD);
    }
    else if (mime == JK_STATUS_MIME_XML) {
        s->start_response(s, 200, "OK", headers_names, headers_vxml, 3);
        jk_puts(s, JK_STATUS_XMLH);
        if (w->doctype) {
            jk_putv(s, w->doctype, "\n", NULL);
        }
        jk_print_xml_start_elt(s, w, 0, 0, "status");
        if (w->xmlns && strlen(w->xmlns))
            jk_putv(s, "  ", w->xmlns, NULL);
        jk_print_xml_stop_elt(s, 0, 0);
    }
    else {
        s->start_response(s, 200, "OK", headers_names, headers_vtxt, 3);
    }

    if (denied == 0) {
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' service allowed for user '%s' [%s] from %s [%s]",
                   w->name,
                   s->remote_user ? s->remote_user : "(null)",
                   s->auth_type ? s->auth_type : "(null)",
                   s->remote_addr ? s->remote_addr : "(null)",
                   s->remote_host ? s->remote_host : "(null)");
    }
    else if (denied == 1) {
        err = "Access denied.";
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' service denied for user '%s' [%s] from %s [%s]",
               w->name,
               s->remote_user ? s->remote_user : "(null)",
               s->auth_type ? s->auth_type : "(null)",
               s->remote_addr ? s->remote_addr : "(null)",
               s->remote_host ? s->remote_host : "(null)");
    }
    else if (denied == 2) {
        err = "Access denied.";
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' service denied (no user) [%s] from %s [%s]",
               w->name,
               s->remote_user ? s->remote_user : "(null)",
               s->auth_type ? s->auth_type : "(null)",
               s->remote_addr ? s->remote_addr : "(null)",
               s->remote_host ? s->remote_host : "(null)");
    }
    else {
        err = "Access denied.";
        jk_log(l, JK_LOG_WARNING,
               "Status worker '%s' service denied (unknown reason) for user '%s' [%s] from %s [%s]",
               w->name,
               s->remote_user ? s->remote_user : "(null)",
               s->auth_type ? s->auth_type : "(null)",
               s->remote_addr ? s->remote_addr : "(null)",
               s->remote_host ? s->remote_host : "(null)");
    }

    if (!err) {
        if (read_only && !(cmd_props & JK_STATUS_CMD_PROP_READONLY)) {
            err = "This command is not allowed in read only mode.";
        }
    }

    if (!err) {
        if (cmd == JK_STATUS_CMD_UNKNOWN) {
            err = "Invalid command.";
        }
        else if (mime == JK_STATUS_MIME_UNKNOWN) {
            err = "Invalid mime type.";
        }
        else if (cmd_props & JK_STATUS_CMD_PROP_CHECK_WORKER &&
                 (check_worker(s, p, cmd_props & JK_STATUS_CMD_PROP_WILDCARD, l) != JK_TRUE)) {
            err = p->msg;
        }
    }

    if (!err) {
        char buf_time[JK_STATUS_TIME_BUF_SZ];
        char buf_tz[JK_STATUS_TIME_BUF_SZ];
        time_t clock = time(NULL);
        long unix_seconds = (long)clock;
        int rc_time = status_strftime(clock, mime, buf_time, buf_tz, l);
        if (cmd == JK_STATUS_CMD_UPDATE) {
            /* lock shared memory */
            jk_shm_lock();
            if (update_worker(s, p, l) == JK_FALSE) {
                if (strncmp("OK", p->msg, 3))
                    err = p->msg;
                else
                    err = "Update failed";
            }
            /* unlock the shared memory */
            jk_shm_unlock();
            if (mime == JK_STATUS_MIME_HTML) {
                write_html_refresh_response(s, p, err, l);
            }
        }
        else if (cmd == JK_STATUS_CMD_RESET) {
            /* lock shared memory */
            jk_shm_lock();
            if (reset_worker(s, p, l) == JK_FALSE) {
                err = "Reset failed";
            }
            /* unlock the shared memory */
            jk_shm_unlock();
            if (mime == JK_STATUS_MIME_HTML) {
                write_html_refresh_response(s, p, err, l);
            }
        }
        else if (cmd == JK_STATUS_CMD_RECOVER) {
            /* lock shared memory */
            jk_shm_lock();
            if (recover_worker(s, p, l) == JK_FALSE) {
                err = "Marking worker for recovery failed";
            }
            /* unlock the shared memory */
            jk_shm_unlock();
            if (mime == JK_STATUS_MIME_HTML) {
                write_html_refresh_response(s, p, err, l);
            }
        }
        else {
            if (mime == JK_STATUS_MIME_XML) {
                jk_print_xml_start_elt(s, w, 0, 0, "server");
                jk_print_xml_att_string(s, 2, "name", s->server_name);
                jk_print_xml_att_int(s, 2, "port", s->server_port);
                jk_print_xml_stop_elt(s, 0, 1);
                if (cmd_props & JK_STATUS_CMD_PROP_HEAD) {
                    if (rc_time > 0 ) {
                        jk_print_xml_start_elt(s, w, 0, 0, "time");
                        jk_print_xml_att_string(s, 2, "datetime", buf_time);
                        jk_print_xml_att_string(s, 2, "tz", buf_tz);
                        jk_print_xml_att_long(s, 2, "unix", unix_seconds);
                        jk_print_xml_stop_elt(s, 0, 1);
                    }
                    jk_print_xml_start_elt(s, w, 0, 0, "software");
                    jk_print_xml_att_string(s, 2, "web_server", s->server_software);
                    jk_print_xml_att_string(s, 2, "jk_version", JK_EXPOSED_VERSION);
                    jk_print_xml_stop_elt(s, 0, 1);
                }
                if (cmd == JK_STATUS_CMD_LIST) {
                    /* Step 2: Display configuration */
                    if (list_workers(s, p, l) != JK_TRUE) {
                        err = "Error in listing the workers.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_SHOW) {
                    /* Step 2: Display detailed configuration */
                    if (show_worker(s, p, l) != JK_TRUE) {
                        err = "Error in showing this worker.";
                    }
                }
            }
            else if (mime == JK_STATUS_MIME_TXT) {
                jk_puts(s, "Server:");
                jk_printf(s, " name=%s", s->server_name);
                jk_printf(s, " port=%d", s->server_port);
                jk_puts(s, "\n");
                if (cmd_props & JK_STATUS_CMD_PROP_HEAD) {
                    if (rc_time > 0) {
                        jk_puts(s, "Time:");
                        jk_printf(s, " datetime=%s", buf_time);
                        jk_printf(s, " tz=%s", buf_tz);
                        jk_printf(s, " unix=%ld", unix_seconds);
                        jk_puts(s, "\n");
                    }
                    jk_puts(s, "Software:");
                    jk_printf(s, " web_server=\"%s\"", s->server_software);
                    jk_printf(s, " jk_version=%s", JK_EXPOSED_VERSION);
                    jk_puts(s, "\n");
                }
                if (cmd == JK_STATUS_CMD_LIST) {
                    /* Step 2: Display configuration */
                    if (list_workers(s, p, l) != JK_TRUE) {
                        err = "Error in listing the workers.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_SHOW) {
                    /* Step 2: Display detailed configuration */
                    if (show_worker(s, p, l) != JK_TRUE) {
                        err = "Error in showing this worker.";
                    }
                }
            }
            else if (mime == JK_STATUS_MIME_PROP) {
                jk_print_prop_att_string(s, w, NULL, "server_name", s->server_name);
                jk_print_prop_att_int(s, w, NULL, "server_port", s->server_port);
                if (cmd_props & JK_STATUS_CMD_PROP_HEAD) {
                    if (rc_time > 0) {
                        jk_print_prop_att_string(s, w, NULL, "time_datetime", buf_time);
                        jk_print_prop_att_string(s, w, NULL, "time_tz", buf_tz);
                        jk_print_prop_att_long(s, w, NULL, "time_unix", unix_seconds);
                    }
                    jk_print_prop_att_string(s, w, NULL, "web_server", s->server_software);
                    jk_print_prop_att_string(s, w, NULL, "jk_version", JK_EXPOSED_VERSION);
                }
                if (cmd == JK_STATUS_CMD_LIST) {
                    /* Step 2: Display configuration */
                    if (list_workers(s, p, l) != JK_TRUE) {
                        err = "Error in listing the workers.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_SHOW) {
                    /* Step 2: Display detailed configuration */
                    if (show_worker(s, p, l) != JK_TRUE) {
                        err = "Error in showing this worker.";
                    }
                }
            }
            else if (mime == JK_STATUS_MIME_HTML) {
                if (cmd_props & JK_STATUS_CMD_PROP_REFRESH &&
                    refresh > 0) {
                    jk_printf(s, "\n<meta http-equiv=\"Refresh\" content=\"%d;url=%s?%s\">",
                          refresh, s->req_uri, p->query_string);
                }
                if (w->css) {
                    jk_putv(s, "\n<link rel=\"stylesheet\" type=\"text/css\" href=\"",
                            w->css, "\" />\n", NULL);
                }
                jk_puts(s, JK_STATUS_HEND);
                jk_puts(s, "<h1>JK Status Manager for ");
                jk_puts(s, s->server_name);
                jk_printf(s, ":%d", s->server_port);
                if (read_only) {
                    jk_puts(s, " (read only)");
                }
                jk_puts(s, "</h1>\n\n");
                if (cmd_props & JK_STATUS_CMD_PROP_HEAD) {
                    jk_putv(s, "<table><tr><td>Server Version:</td><td>",
                            s->server_software, "</td><td>&nbsp;&nbsp;&nbsp;</td><td>", NULL);
                    if (rc_time > 0) {
                        jk_putv(s, "Server Time:</td><td>", buf_time, NULL);
                    }
                    jk_puts(s, "</td></tr>\n");
                    jk_putv(s, "<tr><td>JK Version:</td><td>",
                            JK_EXPOSED_VERSION, "</td><td></td><td>", NULL);
                    jk_printf(s, "Unix Seconds:</td><td>%d", unix_seconds);
                    jk_puts(s, "</td></tr></table>\n<hr/>\n");
                }
                jk_puts(s, "<table><tbody valign=\"baseline\"><tr>\n");
                if (cmd_props & JK_STATUS_CMD_PROP_REFRESH) {
                    jk_puts(s, "<td>");
                    if (refresh > 0) {
                        const char *str = p->query_string;
                        char *buf = jk_pool_alloc(s->pool, sizeof(char *) * (strlen(str)+1));
                        int result = 0;
                        size_t scan = 0;
                        size_t len = strlen(JK_STATUS_ARG_REFRESH);

                        while (str[scan] != '\0') {
                            if (strncmp(&str[scan], JK_STATUS_ARG_REFRESH, len) == 0 &&
                                str[scan+len] == '=') {
                                scan += len + 1;
                                while (str[scan] != '\0' && str[scan] != '&')
                                    scan++;
                                if (str[scan] == '&')
                                    scan++;
                            }
                            else {
                                if (result > 0 && str[scan] != '\0' && str[scan] != '&') {
                                    buf[result] = '&';
                                    result++;
                                }
                                while (str[scan] != '\0' && str[scan] != '&') {
                                    buf[result] = str[scan];
                                    result++;
                                    scan++;
                                }
                                if (str[scan] == '&')
                                    scan++;
                            }
                        }
                        buf[result] = '\0';

                        jk_putv(s, "[<a href=\"", s->req_uri, NULL);
                        if (buf && buf[0])
                            jk_putv(s, "?", buf, NULL);
                        jk_puts(s, "\">Stop auto refresh</a>]");
                    }
                    else {
                        status_start_form(s, p, "get", JK_STATUS_CMD_UNKNOWN, JK_STATUS_ARG_REFRESH, l);
                        jk_puts(s, "<input type=\"submit\" value=\"Start auto refresh\"/>\n");
                        jk_putv(s, "(every ",
                                "<input name=\"", JK_STATUS_ARG_REFRESH,
                                "\" type=\"text\" size=\"3\" value=\"",
                                JK_STATUS_REFRESH_DEF "\"/> ",
                                "seconds)", NULL);
                        jk_puts(s, "</form>\n");
                    }
                    jk_puts(s, "</td><td>&nbsp;&nbsp;|&nbsp;&nbsp;</td>\n");
                }
                if (cmd_props & JK_STATUS_CMD_PROP_FMT) {
                    jk_puts(s, "<td>\n");
                    status_start_form(s, p, "get", JK_STATUS_CMD_UNKNOWN, JK_STATUS_ARG_MIME, l);
                    jk_puts(s, "<input type=\"submit\" value=\"Change format\"/>\n");
                    jk_putv(s, "<select name=\"", JK_STATUS_ARG_MIME, "\" size=\"1\">", NULL);
                    jk_putv(s, "<option value=\"", JK_STATUS_MIME_TEXT_XML, "\">XML</option>", NULL);
                    jk_putv(s, "<option value=\"", JK_STATUS_MIME_TEXT_PROP, "\">Properties</option>", NULL);
                    jk_putv(s, "<option value=\"", JK_STATUS_MIME_TEXT_TXT, "\">Text</option>", NULL);
                    jk_puts(s, "</select></form>\n");
                    jk_puts(s, "</td>\n");
                }
                jk_puts(s, "</tr></table>\n");
                jk_puts(s, "<table><tbody valign=\"baseline\"><tr>\n");
                if (cmd_props & JK_STATUS_CMD_PROP_BACK_LINK) {
                    int from;
                    jk_puts(s, "<td>\n");
                    status_get_string(p, JK_STATUS_ARG_FROM, NULL, &arg, l);
                    from = status_cmd_int(arg);
                    jk_puts(s, "[");
                    if (cmd_props & JK_STATUS_CMD_PROP_BACK_LIST ||
                        from == JK_STATUS_CMD_LIST) {
                        status_write_uri(s, p, "Back to worker list", JK_STATUS_CMD_LIST, JK_STATUS_MIME_UNKNOWN,
                                         "", "", 0, 0, "", l);
                    }
                    else {
                        status_write_uri(s, p, "Back to worker view", JK_STATUS_CMD_SHOW, JK_STATUS_MIME_UNKNOWN,
                                         NULL, NULL, 0, 0, "", l);
                    }
                    jk_puts(s, "]&nbsp;&nbsp;");
                    jk_puts(s, "</td>\n");
                }
                if (cmd_props & JK_STATUS_CMD_PROP_SWITCH_RO) {
                    jk_puts(s, "<td>\n");
                    if (!w->read_only) {
                        jk_puts(s, "[");
                        if (read_only) {
                            status_write_uri(s, p, "Read/Write", 0, JK_STATUS_MIME_UNKNOWN,
                                             NULL, NULL, 0, JK_STATUS_ARG_OPTION_READ_ONLY, NULL, l);
                        }
                        else {
                            status_write_uri(s, p, "Read Only", 0, JK_STATUS_MIME_UNKNOWN,
                                             NULL, NULL, JK_STATUS_ARG_OPTION_READ_ONLY, 0, NULL, l);
                        }
                        jk_puts(s, "]&nbsp;&nbsp;\n");
                    }
                    jk_puts(s, "</td>\n");
                }
                if (cmd_props & JK_STATUS_CMD_PROP_DUMP_LINK) {
                    jk_puts(s, "<td>\n");
                    jk_puts(s, "[");
                    status_write_uri(s, p, "Dump", JK_STATUS_CMD_DUMP, JK_STATUS_MIME_UNKNOWN,
                                     NULL, NULL, 0, 0, NULL, l);
                    jk_puts(s, "]&nbsp;&nbsp;\n");
                    jk_puts(s, "</td>\n");
                }
                if (cmd_props & JK_STATUS_CMD_PROP_LINK_HELP &&
                    (cmd == JK_STATUS_CMD_LIST || !read_only)) {
                    jk_puts(s, "<td>\n");
                    jk_puts(s, "[");
                    if (cmd == JK_STATUS_CMD_LIST) {
                        jk_puts(s, "<b>S</b>=Show only this worker, ");
                    }
                    jk_puts(s, "<b>E</b>=Edit worker, <b>R</b>=Reset worker state, <b>T</b>=Try worker recovery");
                    jk_puts(s, "]<br/>\n");
                    jk_puts(s, "</td>\n");
                }
                jk_puts(s, "</tr></table>\n");
                if (cmd == JK_STATUS_CMD_LIST) {
                    /* Step 2: Display configuration */
                    if (list_workers(s, p, l) != JK_TRUE) {
                        err = "Error in listing the workers.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_SHOW) {
                    /* Step 2: Display detailed configuration */
                    if (show_worker(s, p, l) != JK_TRUE) {
                        err = "Error in showing this worker.";
                    }
                }
                else if (cmd == JK_STATUS_CMD_EDIT) {
                    /* Step 2: Display edit form */
                    if (edit_worker(s, p, l) != JK_TRUE) {
                        err = "Error in generating this worker's configuration form.";
                    }
                }
            }
            if (cmd == JK_STATUS_CMD_DUMP) {
                if (dump_config(s, p, mime, l) == JK_FALSE) {
                    err = "Dumping configuration failed";
                }
            }
            if (cmd_props & JK_STATUS_CMD_PROP_LEGEND) {
                display_legend(s, p, l);
            }
        }
    }
    if (err) {
        jk_log(l, JK_LOG_WARNING, "Status worker '%s': %s", w->name, err);
        if (mime == JK_STATUS_MIME_HTML) {
            jk_putv(s, "<p><b>Result: ERROR - ", err, "</b><br/>", NULL);
            jk_putv(s, "<a href=\"", s->req_uri, "\">JK Status Manager Start Page</a></p>", NULL);
        }
        else if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_start_elt(s, w, 2, 0, "result");
            jk_print_xml_att_string(s, 4, "type", "ERROR");
            jk_print_xml_att_string(s, 4, "message", err);
            jk_print_xml_stop_elt(s, 2, 1);
        }
        else if (mime == JK_STATUS_MIME_TXT) {
            jk_puts(s, "Result:");
            jk_printf(s, " type=%s", "ERROR");
            jk_printf(s, " message=\"%s\"", err);
            jk_puts(s, "\n");
        }
        else {
            jk_print_prop_att_string(s, w, "result", "type", "ERROR");
            jk_print_prop_att_string(s, w, "result", "message", err);
        }
    }
    else {
        if (mime == JK_STATUS_MIME_HTML) {
            jk_putv(s, "<p><a href=\"", s->req_uri, "\">JK Status Manager Start Page</a></p>", NULL);
        }
        else if (mime == JK_STATUS_MIME_XML) {
            jk_print_xml_start_elt(s, w, 2, 0, "result");
            jk_print_xml_att_string(s, 4, "type", "OK");
            jk_print_xml_att_string(s, 4, "message", "Action finished");
            jk_print_xml_stop_elt(s, 2, 1);
        }
        else if (mime == JK_STATUS_MIME_TXT) {
            jk_puts(s, "Result:");
            jk_printf(s, " type=%s", "OK");
            jk_printf(s, " message=\"%s\"", "Action finished");
            jk_puts(s, "\n");
        }
        else {
            jk_print_prop_att_string(s, w, "result", "type", "OK");
            jk_print_prop_att_string(s, w, "result", "message", "Action finished");
        }
    }
    if (mime == JK_STATUS_MIME_HTML) {
        if (w->css) {
            jk_putv(s, "<hr/><div class=\"footer\">", JK_STATUS_COPYRIGHT,
                    "</div>\n", NULL);
        }
        else {
            jk_putv(s, "<hr/><p align=\"center\"><small>", JK_STATUS_COPYRIGHT,
                    "</small></p>\n", NULL);
        }
        jk_puts(s, JK_STATUS_BEND);
    }
    else if (mime == JK_STATUS_MIME_XML) {
        jk_print_xml_close_elt(s, w, 0, "status");
    }
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int JK_METHOD done(jk_endpoint_t **e, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (e && *e && (*e)->endpoint_private) {
        status_endpoint_t *p = (*e)->endpoint_private;

        jk_map_free(&(p->req_params));
        free(p);
        *e = NULL;
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD validate(jk_worker_t *pThis,
                              jk_map_t *props,
                              jk_worker_env_t *we, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && pThis->worker_private) {
        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD init(jk_worker_t *pThis,
                          jk_map_t *props,
                          jk_worker_env_t *we, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);
    if (pThis && pThis->worker_private) {
        status_worker_t *p = pThis->worker_private;
        char **good_rating;
        unsigned int num_of_good;
        char **bad_rating;
        unsigned int num_of_bad;
        unsigned int i;
        p->we = we;
        p->css = jk_get_worker_style_sheet(props, p->name, NULL);
        p->prefix = jk_get_worker_prop_prefix(props, p->name, JK_STATUS_PREFIX_DEF);
        p->ns = jk_get_worker_name_space(props, p->name, JK_STATUS_NS_DEF);
        p->xmlns = jk_get_worker_xmlns(props, p->name, JK_STATUS_XMLNS_DEF);
        p->doctype = jk_get_worker_xml_doctype(props, p->name, NULL);
        p->read_only = jk_get_is_read_only(props, p->name);
        p->user_case_insensitive = jk_get_worker_user_case_insensitive(props, p->name);
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' is %s and has css '%s', prefix '%s', name space '%s', xml name space '%s', document type '%s'",
                   p->name,
                   p->read_only ? "read-only" : "read/write",
                   p->css ? p->css : "(null)",
                   p->prefix ? p->prefix : "(null)",
                   p->ns ? p->ns : "(null)",
                   p->xmlns ? p->xmlns : "(null)",
                   p->doctype ? p->doctype : "(null)");
        if (jk_get_worker_user_list(props, p->name,
                                    &(p->user_names),
                                    &(p->num_of_users)) && p->num_of_users) {
            for (i = 0; i < p->num_of_users; i++) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                            "Status worker '%s' restricting access to user '%s' case %s",
                            p->name, p->user_names[i],
                            p->user_case_insensitive ? "insensitive" : "sensitive");
            }
        }
        if (jk_get_worker_good_rating(props, p->name,
                                      &good_rating,
                                      &num_of_good) && num_of_good) {
            p->good_mask = 0;
            for (i = 0; i < num_of_good; i++) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                            "Status worker '%s' rating as good: '%s'",
                            p->name, good_rating[i]);
                p->good_mask |= status_get_rating(good_rating[i], l);
            }
        }
        else {
            p->good_mask = JK_STATUS_MASK_GOOD_DEF;
        }
        if (jk_get_worker_bad_rating(props, p->name,
                                      &bad_rating,
                                      &num_of_bad) && num_of_bad) {
            p->bad_mask = 0;
            for (i = 0; i < num_of_bad; i++) {
                if (JK_IS_DEBUG_LEVEL(l))
                    jk_log(l, JK_LOG_DEBUG,
                            "Status worker '%s' rating as bad: '%s'",
                            p->name, bad_rating[i]);
                p->bad_mask |= status_get_rating(bad_rating[i], l);
            }
        }
        else {
            p->bad_mask = JK_STATUS_MASK_BAD_DEF;
        }
        if (JK_IS_DEBUG_LEVEL(l))
            jk_log(l, JK_LOG_DEBUG,
                   "Status worker '%s' has good rating for '%08" JK_UINT32_T_HEX_FMT
                   "' and bad rating for '%08" JK_UINT32_T_HEX_FMT "'",
                   p->name,
                   p->good_mask,
                   p->bad_mask);
        if (p->good_mask & p->bad_mask)
            jk_log(l, JK_LOG_WARNING,
                   "Status worker '%s' has non empty intersection '%08" JK_UINT32_T_HEX_FMT
                   "' between good rating for '%08" JK_UINT32_T_HEX_FMT
                   "' and bad rating for '%08" JK_UINT32_T_HEX_FMT "'",
                   p->name,
                   p->good_mask & p->bad_mask,
                   p->good_mask,
                   p->bad_mask);
    }
    JK_TRACE_EXIT(l);
    return JK_TRUE;
}

static int JK_METHOD get_endpoint(jk_worker_t *pThis,
                                  jk_endpoint_t **pend, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && pThis->worker_private && pend) {
        status_endpoint_t *p = (status_endpoint_t *) malloc(sizeof(status_endpoint_t));
        p->worker = pThis->worker_private;
        p->endpoint.endpoint_private = p;
        p->endpoint.service = service;
        p->endpoint.done = done;
        p->req_params = NULL;
        p->msg = NULL;
        *pend = &p->endpoint;

        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }
    else {
        JK_LOG_NULL_PARAMS(l);
    }

    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

static int JK_METHOD destroy(jk_worker_t **pThis, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (pThis && *pThis && (*pThis)->worker_private) {
        status_worker_t *private_data = (*pThis)->worker_private;

        jk_close_pool(&private_data->p);
        free(private_data);

        JK_TRACE_EXIT(l);
        return JK_TRUE;
    }

    JK_LOG_NULL_PARAMS(l);
    JK_TRACE_EXIT(l);
    return JK_FALSE;
}

int JK_METHOD status_worker_factory(jk_worker_t **w,
                                    const char *name, jk_logger_t *l)
{
    JK_TRACE_ENTER(l);

    if (NULL != name && NULL != w) {
        status_worker_t *private_data =
            (status_worker_t *) calloc(1, sizeof(status_worker_t));

        jk_open_pool(&private_data->p,
                        private_data->buf,
                        sizeof(jk_pool_atom_t) * TINY_POOL_SIZE);

        private_data->name = name;

        private_data->worker.worker_private = private_data;
        private_data->worker.validate = validate;
        private_data->worker.init = init;
        private_data->worker.get_endpoint = get_endpoint;
        private_data->worker.destroy = destroy;

        *w = &private_data->worker;
        JK_TRACE_EXIT(l);
        return JK_STATUS_WORKER_TYPE;
    }
    else {
        JK_LOG_NULL_PARAMS(l);
    }

    JK_TRACE_EXIT(l);
    return 0;
}
