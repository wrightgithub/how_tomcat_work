/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Utility routines for Apache proxy */
#include "mod_proxy.h"
#include "ap_mpm.h"
#include "scoreboard.h"
#include "apr_version.h"

#if (APR_MAJOR_VERSION < 1)
#undef apr_socket_create
#define apr_socket_create apr_socket_create_ex
#endif

/* Global balancer counter */
static int lb_workers = 0;
static int lb_workers_limit = 0;

static int proxy_match_ipaddr(struct dirconn_entry *This, request_rec *r);
static int proxy_match_domainname(struct dirconn_entry *This, request_rec *r);
static int proxy_match_hostname(struct dirconn_entry *This, request_rec *r);
static int proxy_match_word(struct dirconn_entry *This, request_rec *r);

APR_IMPLEMENT_OPTIONAL_HOOK_RUN_ALL(proxy, PROXY, int, create_req, 
                                   (request_rec *r, request_rec *pr), (r, pr),
                                   OK, DECLINED)

/* already called in the knowledge that the characters are hex digits */
PROXY_DECLARE(int) ap_proxy_hex2c(const char *x)
{
    int i, ch;

#if !APR_CHARSET_EBCDIC
    ch = x[0];
    if (apr_isdigit(ch))
	i = ch - '0';
    else if (apr_isupper(ch))
	i = ch - ('A' - 10);
    else
	i = ch - ('a' - 10);
    i <<= 4;

    ch = x[1];
    if (apr_isdigit(ch))
	i += ch - '0';
    else if (apr_isupper(ch))
	i += ch - ('A' - 10);
    else
	i += ch - ('a' - 10);
    return i;
#else /*APR_CHARSET_EBCDIC*/
    /* we assume that the hex value refers to an ASCII character
     * so convert to EBCDIC so that it makes sense locally;
     *
     * example:
     *
     * client specifies %20 in URL to refer to a space char;
     * at this point we're called with EBCDIC "20"; after turning
     * EBCDIC "20" into binary 0x20, we then need to assume that 0x20
     * represents an ASCII char and convert 0x20 to EBCDIC, yielding
     * 0x40
     */
    char buf[1];

    if (1 == sscanf(x, "%2x", &i)) {
        buf[0] = i & 0xFF;
        ap_xlate_proto_from_ascii(buf, 1);
        return buf[0];
    }
    else {
        return 0;
    }
#endif /*APR_CHARSET_EBCDIC*/
}

PROXY_DECLARE(void) ap_proxy_c2hex(int ch, char *x)
{
#if !APR_CHARSET_EBCDIC
    int i;

    x[0] = '%';
    i = (ch & 0xF0) >> 4;
    if (i >= 10)
	x[1] = ('A' - 10) + i;
    else
	x[1] = '0' + i;

    i = ch & 0x0F;
    if (i >= 10)
	x[2] = ('A' - 10) + i;
    else
	x[2] = '0' + i;
#else /*APR_CHARSET_EBCDIC*/
    static const char ntoa[] = { "0123456789ABCDEF" };
    char buf[1];

    ch &= 0xFF;

    buf[0] = ch;
    ap_xlate_proto_to_ascii(buf, 1);

    x[0] = '%';
    x[1] = ntoa[(buf[0] >> 4) & 0x0F];
    x[2] = ntoa[buf[0] & 0x0F];
    x[3] = '\0';
#endif /*APR_CHARSET_EBCDIC*/
}

/*
 * canonicalise a URL-encoded string
 */

/*
 * Convert a URL-encoded string to canonical form.
 * It decodes characters which need not be encoded,
 * and encodes those which must be encoded, and does not touch
 * those which must not be touched.
 */
PROXY_DECLARE(char *)ap_proxy_canonenc(apr_pool_t *p, const char *x, int len, enum enctype t,
	int isenc)
{
    int i, j, ch;
    char *y;
    char *allowed;	/* characters which should not be encoded */
    char *reserved;	/* characters which much not be en/de-coded */

/* N.B. in addition to :@&=, this allows ';' in an http path
 * and '?' in an ftp path -- this may be revised
 * 
 * Also, it makes a '+' character in a search string reserved, as
 * it may be form-encoded. (Although RFC 1738 doesn't allow this -
 * it only permits ; / ? : @ = & as reserved chars.)
 */
    if (t == enc_path)
	allowed = "$-_.+!*'(),;:@&=";
    else if (t == enc_search)
	allowed = "$-_.!*'(),;:@&=";
    else if (t == enc_user)
	allowed = "$-_.+!*'(),;@&=";
    else if (t == enc_fpath)
	allowed = "$-_.+!*'(),?:@&=";
    else			/* if (t == enc_parm) */
	allowed = "$-_.+!*'(),?/:@&=";

    if (t == enc_path)
	reserved = "/";
    else if (t == enc_search)
	reserved = "+";
    else
	reserved = "";

    y = apr_palloc(p, 3 * len + 1);

    for (i = 0, j = 0; i < len; i++, j++) {
/* always handle '/' first */
	ch = x[i];
	if (strchr(reserved, ch)) {
	    y[j] = ch;
	    continue;
	}
/* decode it if not already done */
	if (isenc && ch == '%') {
	    if (!apr_isxdigit(x[i + 1]) || !apr_isxdigit(x[i + 2]))
		return NULL;
	    ch = ap_proxy_hex2c(&x[i + 1]);
	    i += 2;
	    if (ch != 0 && strchr(reserved, ch)) {	/* keep it encoded */
		ap_proxy_c2hex(ch, &y[j]);
		j += 2;
		continue;
	    }
	}
/* recode it, if necessary */
	if (!apr_isalnum(ch) && !strchr(allowed, ch)) {
	    ap_proxy_c2hex(ch, &y[j]);
	    j += 2;
	}
	else
	    y[j] = ch;
    }
    y[j] = '\0';
    return y;
}

/*
 * Parses network-location.
 *    urlp           on input the URL; on output the path, after the leading /
 *    user           NULL if no user/password permitted
 *    password       holder for password
 *    host           holder for host
 *    port           port number; only set if one is supplied.
 *
 * Returns an error string.
 */
PROXY_DECLARE(char *)
     ap_proxy_canon_netloc(apr_pool_t *p, char **const urlp, char **userp,
			char **passwordp, char **hostp, apr_port_t *port)
{
    char *addr, *scope_id, *strp, *host, *url = *urlp;
    char *user = NULL, *password = NULL;
    apr_port_t tmp_port;
    apr_status_t rv;

    if (url[0] != '/' || url[1] != '/')
	return "Malformed URL";
    host = url + 2;
    url = strchr(host, '/');
    if (url == NULL)
	url = "";
    else
	*(url++) = '\0';	/* skip seperating '/' */

    /* find _last_ '@' since it might occur in user/password part */
    strp = strrchr(host, '@');

    if (strp != NULL) {
	*strp = '\0';
	user = host;
	host = strp + 1;

/* find password */
	strp = strchr(user, ':');
	if (strp != NULL) {
	    *strp = '\0';
	    password = ap_proxy_canonenc(p, strp + 1, strlen(strp + 1), enc_user, 1);
	    if (password == NULL)
		return "Bad %-escape in URL (password)";
	}

	user = ap_proxy_canonenc(p, user, strlen(user), enc_user, 1);
	if (user == NULL)
	    return "Bad %-escape in URL (username)";
    }
    if (userp != NULL) {
	*userp = user;
    }
    if (passwordp != NULL) {
	*passwordp = password;
    }

    /* Parse the host string to separate host portion from optional port.
     * Perform range checking on port.
     */
    rv = apr_parse_addr_port(&addr, &scope_id, &tmp_port, host, p);
    if (rv != APR_SUCCESS || addr == NULL || scope_id != NULL) {
        return "Invalid host/port";
    }
    if (tmp_port != 0) { /* only update caller's port if port was specified */
        *port = tmp_port;
    }

    ap_str_tolower(addr); /* DNS names are case-insensitive */

    *urlp = url;
    *hostp = addr;

    return NULL;
}

static const char * const lwday[7] =
{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

/*
 * If the date is a valid RFC 850 date or asctime() date, then it
 * is converted to the RFC 1123 format, otherwise it is not modified.
 * This routine is not very fast at doing conversions, as it uses
 * sscanf and sprintf. However, if the date is already correctly
 * formatted, then it exits very quickly.
 */
PROXY_DECLARE(const char *)
     ap_proxy_date_canon(apr_pool_t *p, const char *x1)
{
    char *x = apr_pstrdup(p, x1);
    int wk, mday, year, hour, min, sec, mon;
    char *q, month[4], zone[4], week[4];

    q = strchr(x, ',');
    /* check for RFC 850 date */
    if (q != NULL && q - x > 3 && q[1] == ' ') {
	*q = '\0';
	for (wk = 0; wk < 7; wk++)
	    if (strcmp(x, lwday[wk]) == 0)
		break;
	*q = ',';
	if (wk == 7)
	    return x;		/* not a valid date */
	if (q[4] != '-' || q[8] != '-' || q[11] != ' ' || q[14] != ':' ||
	    q[17] != ':' || strcmp(&q[20], " GMT") != 0)
	    return x;
	if (sscanf(q + 2, "%u-%3s-%u %u:%u:%u %3s", &mday, month, &year,
		   &hour, &min, &sec, zone) != 7)
	    return x;
	if (year < 70)
	    year += 2000;
	else
	    year += 1900;
    }
    else {
/* check for acstime() date */
	if (x[3] != ' ' || x[7] != ' ' || x[10] != ' ' || x[13] != ':' ||
	    x[16] != ':' || x[19] != ' ' || x[24] != '\0')
	    return x;
	if (sscanf(x, "%3s %3s %u %u:%u:%u %u", week, month, &mday, &hour,
		   &min, &sec, &year) != 7)
	    return x;
	for (wk = 0; wk < 7; wk++)
	    if (strcmp(week, apr_day_snames[wk]) == 0)
		break;
	if (wk == 7)
	    return x;
    }

/* check date */
    for (mon = 0; mon < 12; mon++)
	if (strcmp(month, apr_month_snames[mon]) == 0)
	    break;
    if (mon == 12)
	return x;

    q = apr_palloc(p, 30);
    apr_snprintf(q, 30, "%s, %.2d %s %d %.2d:%.2d:%.2d GMT", apr_day_snames[wk],
       mday, apr_month_snames[mon], year, hour, min, sec);
    return q;
}

PROXY_DECLARE(request_rec *)ap_proxy_make_fake_req(conn_rec *c, request_rec *r)
{
    request_rec *rp = apr_pcalloc(c->pool, sizeof(*r));

    rp->pool            = c->pool;
    rp->status          = HTTP_OK;

    rp->headers_in      = apr_table_make(c->pool, 50);
    rp->subprocess_env  = apr_table_make(c->pool, 50);
    rp->headers_out     = apr_table_make(c->pool, 12);
    rp->err_headers_out = apr_table_make(c->pool, 5);
    rp->notes           = apr_table_make(c->pool, 5);

    rp->server = r->server;
    rp->proxyreq = r->proxyreq;
    rp->request_time = r->request_time;
    rp->connection      = c;
    rp->output_filters  = c->output_filters;
    rp->input_filters   = c->input_filters;
    rp->proto_output_filters  = c->output_filters;
    rp->proto_input_filters   = c->input_filters;

    rp->request_config  = ap_create_request_config(c->pool);
    proxy_run_create_req(r, rp);

    return rp;
}


/*
 * list is a comma-separated list of case-insensitive tokens, with
 * optional whitespace around the tokens.
 * The return returns 1 if the token val is found in the list, or 0
 * otherwise.
 */
PROXY_DECLARE(int) ap_proxy_liststr(const char *list, const char *val)
{
    int len, i;
    const char *p;

    len = strlen(val);

    while (list != NULL) {
	p = ap_strchr_c(list, ',');
	if (p != NULL) {
	    i = p - list;
	    do
		p++;
	    while (apr_isspace(*p));
	}
	else
	    i = strlen(list);

	while (i > 0 && apr_isspace(list[i - 1]))
	    i--;
	if (i == len && strncasecmp(list, val, len) == 0)
	    return 1;
	list = p;
    }
    return 0;
}

/*
 * list is a comma-separated list of case-insensitive tokens, with
 * optional whitespace around the tokens.
 * if val appears on the list of tokens, it is removed from the list,
 * and the new list is returned.
 */
PROXY_DECLARE(char *)ap_proxy_removestr(apr_pool_t *pool, const char *list, const char *val)
{
    int len, i;
    const char *p;
    char *new = NULL;

    len = strlen(val);

    while (list != NULL) {
	p = ap_strchr_c(list, ',');
	if (p != NULL) {
	    i = p - list;
	    do
		p++;
	    while (apr_isspace(*p));
	}
	else
	    i = strlen(list);

	while (i > 0 && apr_isspace(list[i - 1]))
	    i--;
	if (i == len && strncasecmp(list, val, len) == 0) {
	    /* do nothing */
	}
	else {
	    if (new)
		new = apr_pstrcat(pool, new, ",", apr_pstrndup(pool, list, i), NULL);
	    else
		new = apr_pstrndup(pool, list, i);
	}
	list = p;
    }
    return new;
}

/*
 * Converts 8 hex digits to a time integer
 */
PROXY_DECLARE(int) ap_proxy_hex2sec(const char *x)
{
    int i, ch;
    unsigned int j;

    for (i = 0, j = 0; i < 8; i++) {
	ch = x[i];
	j <<= 4;
	if (apr_isdigit(ch))
	    j |= ch - '0';
	else if (apr_isupper(ch))
	    j |= ch - ('A' - 10);
	else
	    j |= ch - ('a' - 10);
    }
    if (j == 0xffffffff)
	return -1;		/* so that it works with 8-byte ints */
    else
	return j;
}

/*
 * Converts a time integer to 8 hex digits
 */
PROXY_DECLARE(void) ap_proxy_sec2hex(int t, char *y)
{
    int i, ch;
    unsigned int j = t;

    for (i = 7; i >= 0; i--) {
	ch = j & 0xF;
	j >>= 4;
	if (ch >= 10)
	    y[i] = ch + ('A' - 10);
	else
	    y[i] = ch + '0';
    }
    y[8] = '\0';
}

PROXY_DECLARE(int) ap_proxyerror(request_rec *r, int statuscode, const char *message)
{
    apr_table_setn(r->notes, "error-notes",
	apr_pstrcat(r->pool, 
		"The proxy server could not handle the request "
		"<em><a href=\"", ap_escape_uri(r->pool, r->uri),
		"\">", ap_escape_html(r->pool, r->method),
		"&nbsp;", 
		ap_escape_html(r->pool, r->uri), "</a></em>.<p>\n"
		"Reason: <strong>",
		ap_escape_html(r->pool, message), 
		"</strong></p>", NULL));

    /* Allow "error-notes" string to be printed by ap_send_error_response() */
    apr_table_setn(r->notes, "verbose-error-to", apr_pstrdup(r->pool, "*"));

    r->status_line = apr_psprintf(r->pool, "%3.3u Proxy Error", statuscode);
    ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
			 "proxy: %s returned by %s", message, r->uri);
    return statuscode;
}

static const char *
     proxy_get_host_of_request(request_rec *r)
{
    char *url, *user = NULL, *password = NULL, *err, *host;
    apr_port_t port;

    if (r->hostname != NULL)
	return r->hostname;

    /* Set url to the first char after "scheme://" */
    if ((url = strchr(r->uri, ':')) == NULL
	|| url[1] != '/' || url[2] != '/')
	return NULL;

    url = apr_pstrdup(r->pool, &url[1]);	/* make it point to "//", which is what proxy_canon_netloc expects */

    err = ap_proxy_canon_netloc(r->pool, &url, &user, &password, &host, &port);

    if (err != NULL)
	ap_log_rerror(APLOG_MARK, APLOG_ERR, 0, r,
		     "%s", err);

    r->hostname = host;

    return host;		/* ought to return the port, too */
}

/* Return TRUE if addr represents an IP address (or an IP network address) */
PROXY_DECLARE(int) ap_proxy_is_ipaddr(struct dirconn_entry *This, apr_pool_t *p)
{
    const char *addr = This->name;
    long ip_addr[4];
    int i, quads;
    long bits;

    /* if the address is given with an explicit netmask, use that */
    /* Due to a deficiency in apr_inet_addr(), it is impossible to parse */
    /* "partial" addresses (with less than 4 quads) correctly, i.e.  */
    /* 192.168.123 is parsed as 192.168.0.123, which is not what I want. */
    /* I therefore have to parse the IP address manually: */
    /*if (proxy_readmask(This->name, &This->addr.s_addr, &This->mask.s_addr) == 0) */
    /* addr and mask were set by proxy_readmask() */
    /*return 1; */

    /* Parse IP addr manually, optionally allowing */
    /* abbreviated net addresses like 192.168. */

    /* Iterate over up to 4 (dotted) quads. */
    for (quads = 0; quads < 4 && *addr != '\0'; ++quads) {
	char *tmp;

	if (*addr == '/' && quads > 0)	/* netmask starts here. */
	    break;

	if (!apr_isdigit(*addr))
	    return 0;		/* no digit at start of quad */

	ip_addr[quads] = strtol(addr, &tmp, 0);

	if (tmp == addr)	/* expected a digit, found something else */
	    return 0;

	if (ip_addr[quads] < 0 || ip_addr[quads] > 255) {
	    /* invalid octet */
	    return 0;
	}

	addr = tmp;

	if (*addr == '.' && quads != 3)
	    ++addr;		/* after the 4th quad, a dot would be illegal */
    }

    for (This->addr.s_addr = 0, i = 0; i < quads; ++i)
	This->addr.s_addr |= htonl(ip_addr[i] << (24 - 8 * i));

    if (addr[0] == '/' && apr_isdigit(addr[1])) {	/* net mask follows: */
	char *tmp;

	++addr;

	bits = strtol(addr, &tmp, 0);

	if (tmp == addr)	/* expected a digit, found something else */
	    return 0;

	addr = tmp;

	if (bits < 0 || bits > 32)	/* netmask must be between 0 and 32 */
	    return 0;

    }
    else {
	/* Determine (i.e., "guess") netmask by counting the */
	/* number of trailing .0's; reduce #quads appropriately */
	/* (so that 192.168.0.0 is equivalent to 192.168.)        */
	while (quads > 0 && ip_addr[quads - 1] == 0)
	    --quads;

	/* "IP Address should be given in dotted-quad form, optionally followed by a netmask (e.g., 192.168.111.0/24)"; */
	if (quads < 1)
	    return 0;

	/* every zero-byte counts as 8 zero-bits */
	bits = 8 * quads;

	if (bits != 32)		/* no warning for fully qualified IP address */
            ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
	      "Warning: NetMask not supplied with IP-Addr; guessing: %s/%ld\n",
		 inet_ntoa(This->addr), bits);
    }

    This->mask.s_addr = htonl(APR_INADDR_NONE << (32 - bits));

    if (*addr == '\0' && (This->addr.s_addr & ~This->mask.s_addr) != 0) {
        ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
	    "Warning: NetMask and IP-Addr disagree in %s/%ld\n",
		inet_ntoa(This->addr), bits);
	This->addr.s_addr &= This->mask.s_addr;
        ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
	    "         Set to %s/%ld\n",
		inet_ntoa(This->addr), bits);
    }

    if (*addr == '\0') {
	This->matcher = proxy_match_ipaddr;
	return 1;
    }
    else
	return (*addr == '\0');	/* okay iff we've parsed the whole string */
}

/* Return TRUE if addr represents an IP address (or an IP network address) */
static int proxy_match_ipaddr(struct dirconn_entry *This, request_rec *r)
{
    int i, ip_addr[4];
    struct in_addr addr, *ip;
    const char *host = proxy_get_host_of_request(r);

    if (host == NULL)   /* oops! */
       return 0;

    memset(&addr, '\0', sizeof addr);
    memset(ip_addr, '\0', sizeof ip_addr);

    if (4 == sscanf(host, "%d.%d.%d.%d", &ip_addr[0], &ip_addr[1], &ip_addr[2], &ip_addr[3])) {
	for (addr.s_addr = 0, i = 0; i < 4; ++i)
	    addr.s_addr |= htonl(ip_addr[i] << (24 - 8 * i));

	if (This->addr.s_addr == (addr.s_addr & This->mask.s_addr)) {
#if DEBUGGING
        ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "1)IP-Match: %s[%s] <-> ", host, inet_ntoa(addr));
        ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "%s/", inet_ntoa(This->addr));
        ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "%s", inet_ntoa(This->mask));
#endif
	    return 1;
	}
#if DEBUGGING
	else {
        ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "1)IP-NoMatch: %s[%s] <-> ", host, inet_ntoa(addr));
        ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "%s/", inet_ntoa(This->addr));
        ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                         "%s", inet_ntoa(This->mask));
	}
#endif
    }
    else {
	struct apr_sockaddr_t *reqaddr;

        if (apr_sockaddr_info_get(&reqaddr, host, APR_UNSPEC, 0, 0, r->pool)
	    != APR_SUCCESS) {
#if DEBUGGING
	    ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
			 "2)IP-NoMatch: hostname=%s msg=Host not found", 
			 host);
#endif
	    return 0;
	}

	/* Try to deal with multiple IP addr's for a host */
	/* FIXME: This needs to be able to deal with IPv6 */
	while (reqaddr) {
	    ip = (struct in_addr *) reqaddr->ipaddr_ptr;
	    if (This->addr.s_addr == (ip->s_addr & This->mask.s_addr)) {
#if DEBUGGING
		ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
			     "3)IP-Match: %s[%s] <-> ", host, 
			     inet_ntoa(*ip));
		ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
			     "%s/", inet_ntoa(This->addr));
		ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
			     "%s", inet_ntoa(This->mask));
#endif
		return 1;
	    }
#if DEBUGGING
	    else {
                ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
			     "3)IP-NoMatch: %s[%s] <-> ", host, 
			     inet_ntoa(*ip));
                ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
			     "%s/", inet_ntoa(This->addr));
                ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
			     "%s", inet_ntoa(This->mask));
	    }
#endif
	    reqaddr = reqaddr->next;
	}
    }

    return 0;
}

/* Return TRUE if addr represents a domain name */
PROXY_DECLARE(int) ap_proxy_is_domainname(struct dirconn_entry *This, apr_pool_t *p)
{
    char *addr = This->name;
    int i;

    /* Domain name must start with a '.' */
    if (addr[0] != '.')
        return 0;

    /* rfc1035 says DNS names must consist of "[-a-zA-Z0-9]" and '.' */
    for (i = 0; apr_isalnum(addr[i]) || addr[i] == '-' || addr[i] == '.'; ++i)
        continue;

#if 0
    if (addr[i] == ':') {
    ap_log_error(APLOG_MARK, APLOG_STARTUP, 0, NULL,
                     "@@@@ handle optional port in proxy_is_domainname()");
	/* @@@@ handle optional port */
    }
#endif

    if (addr[i] != '\0')
        return 0;

    /* Strip trailing dots */
    for (i = strlen(addr) - 1; i > 0 && addr[i] == '.'; --i)
        addr[i] = '\0';

    This->matcher = proxy_match_domainname;
    return 1;
}

/* Return TRUE if host "host" is in domain "domain" */
static int proxy_match_domainname(struct dirconn_entry *This, request_rec *r)
{
    const char *host = proxy_get_host_of_request(r);
    int d_len = strlen(This->name), h_len;

    if (host == NULL)		/* some error was logged already */
        return 0;

    h_len = strlen(host);

    /* @@@ do this within the setup? */
    /* Ignore trailing dots in domain comparison: */
    while (d_len > 0 && This->name[d_len - 1] == '.')
        --d_len;
    while (h_len > 0 && host[h_len - 1] == '.')
        --h_len;
    return h_len > d_len
        && strncasecmp(&host[h_len - d_len], This->name, d_len) == 0;
}

/* Return TRUE if host represents a host name */
PROXY_DECLARE(int) ap_proxy_is_hostname(struct dirconn_entry *This, apr_pool_t *p)
{
    struct apr_sockaddr_t *addr;
    char *host = This->name;
    int i;

    /* Host names must not start with a '.' */
    if (host[0] == '.')
        return 0;

    /* rfc1035 says DNS names must consist of "[-a-zA-Z0-9]" and '.' */
    for (i = 0; apr_isalnum(host[i]) || host[i] == '-' || host[i] == '.'; ++i);

    if (host[i] != '\0' || apr_sockaddr_info_get(&addr, host, APR_UNSPEC, 0, 0, p) != APR_SUCCESS)
        return 0;
    
    This->hostaddr = addr;

    /* Strip trailing dots */
    for (i = strlen(host) - 1; i > 0 && host[i] == '.'; --i)
        host[i] = '\0';

    This->matcher = proxy_match_hostname;
    return 1;
}

/* Return TRUE if host "host" is equal to host2 "host2" */
static int proxy_match_hostname(struct dirconn_entry *This, request_rec *r)
{
    char *host = This->name;
    const char *host2 = proxy_get_host_of_request(r);
    int h2_len;
    int h1_len;

    if (host == NULL || host2 == NULL)
        return 0; /* oops! */

    h2_len = strlen(host2);
    h1_len = strlen(host);

#if 0
    struct apr_sockaddr_t *addr = *This->hostaddr;

    /* Try to deal with multiple IP addr's for a host */
    while (addr) {
        if (addr->ipaddr_ptr == ? ? ? ? ? ? ? ? ? ? ? ? ?)
            return 1;
        addr = addr->next;
    }
#endif

    /* Ignore trailing dots in host2 comparison: */
    while (h2_len > 0 && host2[h2_len - 1] == '.')
        --h2_len;
    while (h1_len > 0 && host[h1_len - 1] == '.')
        --h1_len;
    return h1_len == h2_len
        && strncasecmp(host, host2, h1_len) == 0;
}

/* Return TRUE if addr is to be matched as a word */
PROXY_DECLARE(int) ap_proxy_is_word(struct dirconn_entry *This, apr_pool_t *p)
{
    This->matcher = proxy_match_word;
    return 1;
}

/* Return TRUE if string "str2" occurs literally in "str1" */
static int proxy_match_word(struct dirconn_entry *This, request_rec *r)
{
    const char *host = proxy_get_host_of_request(r);
    return host != NULL && ap_strstr_c(host, This->name) != NULL;
}

/* checks whether a host in uri_addr matches proxyblock */
PROXY_DECLARE(int) ap_proxy_checkproxyblock(request_rec *r, proxy_server_conf *conf, 
                             apr_sockaddr_t *uri_addr)
{
    int j;
    apr_sockaddr_t * src_uri_addr = uri_addr;
    /* XXX FIXME: conf->noproxies->elts is part of an opaque structure */
    for (j = 0; j < conf->noproxies->nelts; j++) {
        struct noproxy_entry *npent = (struct noproxy_entry *) conf->noproxies->elts;
        struct apr_sockaddr_t *conf_addr = npent[j].addr;
        uri_addr = src_uri_addr;
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                     "proxy: checking remote machine [%s] against [%s]", uri_addr->hostname, npent[j].name);
        if ((npent[j].name && ap_strstr_c(uri_addr->hostname, npent[j].name))
            || npent[j].name[0] == '*') {
            ap_log_error(APLOG_MARK, APLOG_WARNING, 0, r->server,
                         "proxy: connect to remote machine %s blocked: name %s matched", uri_addr->hostname, npent[j].name);
            return HTTP_FORBIDDEN;
        }
        while (conf_addr) {
            while (uri_addr) {
                char *conf_ip;
                char *uri_ip;
                apr_sockaddr_ip_get(&conf_ip, conf_addr);
                apr_sockaddr_ip_get(&uri_ip, uri_addr);
                ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                             "proxy: ProxyBlock comparing %s and %s", conf_ip, uri_ip);
                if (!apr_strnatcasecmp(conf_ip, uri_ip)) {
                    ap_log_error(APLOG_MARK, APLOG_WARNING, 0, r->server,
                                 "proxy: connect to remote machine %s blocked: IP %s matched", uri_addr->hostname, conf_ip);
                    return HTTP_FORBIDDEN;
                }
                uri_addr = uri_addr->next;
            }
            conf_addr = conf_addr->next;
        }
    }
    return OK;
}

/* set up the minimal filter set */
PROXY_DECLARE(int) ap_proxy_pre_http_request(conn_rec *c, request_rec *r)
{
    ap_add_input_filter("HTTP_IN", NULL, r, c);
    return OK;
}

/* converts a series of buckets into a string 
 * XXX: BillS says this function performs essentially the same function as 
 * ap_rgetline() in protocol.c. Deprecate this function and use ap_rgetline() 
 * instead? I think ap_proxy_string_read() will not work properly on non ASCII
 * (EBCDIC) machines either.
 */
PROXY_DECLARE(apr_status_t) ap_proxy_string_read(conn_rec *c, apr_bucket_brigade *bb,
                                                 char *buff, apr_size_t bufflen, int *eos)
{
    apr_bucket *e;
    apr_status_t rv;
    char *pos = buff;
    char *response;
    int found = 0;
    apr_size_t len;

    /* start with an empty string */
    buff[0] = 0;
    *eos = 0;

    /* loop through each brigade */
    while (!found) {
        /* get brigade from network one line at a time */
        if (APR_SUCCESS != (rv = ap_get_brigade(c->input_filters, bb, 
                                                AP_MODE_GETLINE,
                                                APR_BLOCK_READ,
                                                0))) {
            return rv;
        }
        /* loop through each bucket */
        while (!found) {
            if (*eos || APR_BRIGADE_EMPTY(bb)) {
                /* The connection aborted or timed out */
                return APR_ECONNABORTED;
            }
            e = APR_BRIGADE_FIRST(bb);
            if (APR_BUCKET_IS_EOS(e)) {
                *eos = 1;
            }
            else {
                if (APR_SUCCESS != apr_bucket_read(e, (const char **)&response, &len, APR_BLOCK_READ)) {
                    return rv;
                }
                /* is string LF terminated? 
                 * XXX: This check can be made more efficient by simply checking 
                 * if the last character in the 'response' buffer is an ASCII_LF.
                 * See ap_rgetline() for an example.
                 */
                if (memchr(response, APR_ASCII_LF, len)) {
                    found = 1;
                }
                /* concat strings until buff is full - then throw the data away */
                if (len > ((bufflen-1)-(pos-buff))) {
                    len = (bufflen-1)-(pos-buff);
                }
                if (len > 0) {
                    pos = apr_cpystrn(pos, response, len);
                }
            }
            APR_BUCKET_REMOVE(e);
            apr_bucket_destroy(e);
        }
    }

    return APR_SUCCESS;
}

/* unmerge an element in the table */
PROXY_DECLARE(void) ap_proxy_table_unmerge(apr_pool_t *p, apr_table_t *t, char *key)
{
    apr_off_t offset = 0;
    apr_off_t count = 0;
    char *value = NULL;

    /* get the value to unmerge */
    const char *initial = apr_table_get(t, key);
    if (!initial) {
        return;
    }
    value = apr_pstrdup(p, initial);

    /* remove the value from the headers */
    apr_table_unset(t, key);

    /* find each comma */
    while (value[count]) {
        if (value[count] == ',') {
            value[count] = 0;
            apr_table_add(t, key, value + offset);
            offset = count + 1;
        }
        count++;
    }
    apr_table_add(t, key, value + offset);
}

PROXY_DECLARE(proxy_balancer *) ap_proxy_get_balancer(apr_pool_t *p,
                                                      proxy_server_conf *conf,
                                                      const char *url)
{
    proxy_balancer *balancer;
    char *c, *uri = apr_pstrdup(p, url);
    int i;
    
    c = strchr(uri, ':');   
    if (c == NULL || c[1] != '/' || c[2] != '/' || c[3] == '\0')
       return NULL;
    /* remove path from uri */
    if ((c = strchr(c + 3, '/')))
        *c = '\0';
    balancer = (proxy_balancer *)conf->balancers->elts;
    for (i = 0; i < conf->balancers->nelts; i++) {
        if (strcasecmp(balancer->name, uri) == 0)
            return balancer;
        balancer++;
    }
    return NULL;
}

PROXY_DECLARE(const char *) ap_proxy_add_balancer(proxy_balancer **balancer,
                                                  apr_pool_t *p,
                                                  proxy_server_conf *conf,
                                                  const char *url)
{
    char *c, *q, *uri = apr_pstrdup(p, url);
    apr_status_t rc = 0;

    c = strchr(uri, ':');   
    if (c == NULL || c[1] != '/' || c[2] != '/' || c[3] == '\0')
       return "Bad syntax for a balancer name";
    /* remove path from uri */
    if ((q = strchr(c + 3, '/')))
        *q = '\0';

    ap_str_tolower(uri);
    *balancer = apr_array_push(conf->balancers);
    (*balancer)->name = uri;
    (*balancer)->workers = apr_array_make(p, 5, sizeof(proxy_runtime_worker));
    /* XXX Is this a right place to create mutex */
#if APR_HAS_THREADS
    if ((rc = apr_thread_mutex_create(&((*balancer)->mutex),
                APR_THREAD_MUTEX_DEFAULT, p)) != APR_SUCCESS) {
            /* XXX: Do we need to log something here */
            return "can not create thread mutex";
    }
#endif

    return NULL;
}

PROXY_DECLARE(proxy_worker *) ap_proxy_get_worker(apr_pool_t *p,
                                                  proxy_server_conf *conf,
                                                  const char *url)
{
    proxy_worker *worker;
    char *c, *uri = apr_pstrdup(p, url);
    int i;
    
    c = strchr(uri, ':');   
    if (c == NULL || c[1] != '/' || c[2] != '/' || c[3] == '\0')
       return NULL;
    /* remove path from uri */
    if ((c = strchr(c + 3, '/')))
        *c = '\0';

    worker = (proxy_worker *)conf->workers->elts;
    for (i = 0; i < conf->workers->nelts; i++) {
        if (strcasecmp(worker->name, uri) == 0) {
            return worker;
        }
        worker++;
    }
    return NULL;
}

static apr_status_t conn_pool_cleanup(void *thepool)
{
    proxy_conn_pool *cp = (proxy_conn_pool *)thepool;
    /* Close the socket */
    cp->addr = NULL;
    return APR_SUCCESS;
}

static void init_conn_pool(apr_pool_t *p, proxy_worker *worker)
{
    apr_pool_t *pool;
    proxy_conn_pool *cp;
    
    /* Create a connection pool's subpool. 
     * This pool is used for connection recycling.
     * Once the worker is added it is never removed but
     * it can be disabled.
     */
    apr_pool_create(&pool, p);
    /* Alloc from the same pool as worker.
     * proxy_conn_pool is permanently attached to the worker. 
     */
    cp = (proxy_conn_pool *)apr_pcalloc(p, sizeof(proxy_conn_pool));
    cp->pool = pool;    
    worker->cp = cp;
    apr_pool_cleanup_register(p, (void *)cp,
                              conn_pool_cleanup,
                              apr_pool_cleanup_null);      

}

PROXY_DECLARE(const char *) ap_proxy_add_worker(proxy_worker **worker,
                                                apr_pool_t *p,
                                                proxy_server_conf *conf,
                                                const char *url)
{
    char *c, *q, *uri = apr_pstrdup(p, url);
    int port;
    
    c = strchr(uri, ':');   
    if (c == NULL || c[1] != '/' || c[2] != '/' || c[3] == '\0')
       return "Bad syntax for a remote proxy server";
    /* remove path from uri */
    if ((q = strchr(c + 3, '/')))
        *q = '\0';

    q = strchr(c + 3, ':');
    if (q != NULL) {
        if (sscanf(q + 1, "%u", &port) != 1 || port > 65535) {
            return "Bad syntax for a remote proxy server (bad port number)";
        }
    }
    else
        port = -1;
    ap_str_tolower(uri);
    *worker = apr_array_push(conf->workers);
    memset(*worker, 0, sizeof(proxy_worker));
    (*worker)->name = apr_pstrdup(p, uri);
    *c = '\0';
    (*worker)->scheme = uri;
    (*worker)->hostname = c + 3;

    if (port == -1)
        port = apr_uri_port_of_scheme((*worker)->scheme);
    (*worker)->port = port;

    init_conn_pool(p, *worker);

    return NULL;
}

PROXY_DECLARE(void) 
ap_proxy_add_worker_to_balancer(apr_pool_t *pool, proxy_balancer *balancer, proxy_worker *worker)
{
    int i;
    double median, ffactor = 0.0;
    proxy_runtime_worker *runtime, *workers;    
#if PROXY_HAS_SCOREBOARD
    lb_score *score;
#else
    void *score;
#endif

#if PROXY_HAS_SCOREBOARD
    int mpm_daemons;

    ap_mpm_query(AP_MPMQ_HARD_LIMIT_DAEMONS, &mpm_daemons);
    /* Check if we are prefork or single child */
    if (worker->hmax && mpm_daemons > 1) {
        /* Check only if workers_limit is set */
        if (lb_workers_limit && (lb_workers + 1) > lb_workers_limit) {
            ap_log_perror(APLOG_MARK, APLOG_ERR, 0, pool,
                          "proxy: Can not add worker (%s) to balancer (%s)."
                          " Dynamic limit reached.",
                          worker->name, balancer->name);
            return;
        }
        score = ap_get_scoreboard_lb(getpid(), lb_workers);
    }
    else
#endif
    {
        /* Use the plain memory */
        score = apr_pcalloc(pool, sizeof(proxy_runtime_stat));
    }
    if (!score)
        return;
    runtime = apr_array_push(balancer->workers);
    runtime->w = worker;
    runtime->s = (proxy_runtime_stat *)score;
    runtime->s->id = lb_workers;
    /* TODO: deal with the dynamic overflow */
    ++lb_workers;

    /* Recalculate lbfactors */
    workers = (proxy_runtime_worker *)balancer->workers->elts;

    for (i = 0; i < balancer->workers->nelts; i++) {
        /* Set to the original configuration */
        workers[i].s->lbfactor = workers[i].w->lbfactor;
        ffactor += workers[i].s->lbfactor;
    }
    if (ffactor < 100.0) {
        int z = 0;
        for (i = 0; i < balancer->workers->nelts; i++) {
            if (workers[i].s->lbfactor == 0.0) 
                ++z;
        }
        if (z) {
            median = (100.0 - ffactor) / z;
            for (i = 0; i < balancer->workers->nelts; i++) {
                if (workers[i].s->lbfactor == 0.0) 
                    workers[i].s->lbfactor = median;
            }
        }
        else {
            median = (100.0 - ffactor) / balancer->workers->nelts;
            for (i = 0; i < balancer->workers->nelts; i++)
                workers[i].s->lbfactor += median;
        }
    }
    else if (ffactor > 100.0) {
        median = (ffactor - 100.0) / balancer->workers->nelts;
        for (i = 0; i < balancer->workers->nelts; i++) {
            if (workers[i].s->lbfactor > median)
                workers[i].s->lbfactor -= median;
        }
    } 
    for (i = 0; i < balancer->workers->nelts; i++) {
        /* Update the status entires */
        workers[i].s->lbstatus = workers[i].s->lbfactor;
    }
}

PROXY_DECLARE(int) ap_proxy_pre_request(proxy_worker **worker,
                                        proxy_balancer **balancer,
                                        request_rec *r,
                                        proxy_server_conf *conf, char **url)
{
    int access_status;

    access_status = proxy_run_pre_request(worker, balancer, r, conf, url);
    if (access_status == DECLINED && *balancer == NULL) {
        *worker = ap_proxy_get_worker(r->pool, conf, *url);
        if (*worker) {
            *balancer = NULL;
            access_status = OK;
        }
        else
            access_status = DECLINED;
    }
    else if (access_status == DECLINED && balancer != NULL) {
        /* All the workers are busy */
        access_status = HTTP_SERVICE_UNAVAILABLE;
    }
    return access_status;
}

PROXY_DECLARE(int) ap_proxy_post_request(proxy_worker *worker,
                                         proxy_balancer *balancer,
                                         request_rec *r,
                                         proxy_server_conf *conf)
{
    int access_status;
    if (balancer)
        access_status = proxy_run_post_request(worker, balancer, r, conf);
    else { 
        

        access_status = OK;
    }

    return access_status;
}

/* DEPRECATED */
PROXY_DECLARE(int) ap_proxy_connect_to_backend(apr_socket_t **newsock,
                                               const char *proxy_function,
                                               apr_sockaddr_t *backend_addr,
                                               const char *backend_name,
                                               proxy_server_conf *conf,
                                               server_rec *s,
                                               apr_pool_t *p)
{
    apr_status_t rv;
    int connected = 0;
    int loglevel;
    
    while (backend_addr && !connected) {
        if ((rv = apr_socket_create(newsock, backend_addr->family,
                                    SOCK_STREAM, 0, p)) != APR_SUCCESS) {
            loglevel = backend_addr->next ? APLOG_DEBUG : APLOG_ERR;
            ap_log_error(APLOG_MARK, loglevel, rv, s,
                         "proxy: %s: error creating fam %d socket for target %s",
                         proxy_function,
                         backend_addr->family,
                         backend_name);
            /* this could be an IPv6 address from the DNS but the
             * local machine won't give us an IPv6 socket; hopefully the
             * DNS returned an additional address to try
             */
            backend_addr = backend_addr->next;
            continue;
        }

#if !defined(TPF) && !defined(BEOS)
        if (conf->recv_buffer_size > 0 &&
            (rv = apr_socket_opt_set(*newsock, APR_SO_RCVBUF,
                                     conf->recv_buffer_size))) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                         "apr_socket_opt_set(SO_RCVBUF): Failed to set "
                         "ProxyReceiveBufferSize, using default");
        }
#endif

        /* Set a timeout on the socket */
        if (conf->timeout_set == 1) {
            apr_socket_timeout_set(*newsock, conf->timeout);
        }
        else {
             apr_socket_timeout_set(*newsock, s->timeout);
        }

        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "proxy: %s: fam %d socket created to connect to %s",
                     proxy_function, backend_addr->family, backend_name);

        /* make the connection out of the socket */
        rv = apr_socket_connect(*newsock, backend_addr);

        /* if an error occurred, loop round and try again */
        if (rv != APR_SUCCESS) {
            apr_socket_close(*newsock);
            loglevel = backend_addr->next ? APLOG_DEBUG : APLOG_ERR;
            ap_log_error(APLOG_MARK, loglevel, rv, s,
                         "proxy: %s: attempt to connect to %pI (%s) failed",
                         proxy_function,
                         backend_addr,
                         backend_name);
            backend_addr = backend_addr->next;
            continue;
        }
        connected = 1;
    }
    return connected ? 0 : 1;
}

static apr_status_t proxy_conn_cleanup(void *theconn)
{
    proxy_conn_rec *conn = (proxy_conn_rec *)theconn;
    /* Close the socket */
    if (conn->sock)
        apr_socket_close(conn->sock);
    conn->sock = NULL;
    conn->pool = NULL;
    return APR_SUCCESS;
}

static apr_status_t connection_cleanup(void *theconn)
{
    proxy_conn_rec *conn = (proxy_conn_rec *)theconn;
    proxy_worker *worker = conn->worker;
    
    /* deterimine if the connection need to be closed */
    if (conn->close_on_recycle) {
        if (conn->sock)
            apr_socket_close(conn->sock);
        conn->sock = NULL;
    }
#if APR_HAS_THREADS
    if (worker->hmax && worker->cp->res) {
        apr_reslist_release(worker->cp->res, (void *)conn);
    }
    else
#endif
    {
        worker->cp->conn = conn;
    }

    /* Allways return the SUCCESS */
    return APR_SUCCESS;
}

/* reslist constructor */
static apr_status_t connection_constructor(void **resource, void *params,
                                           apr_pool_t *pool)
{
    apr_pool_t *ctx;
    proxy_conn_rec *conn;
    server_rec *s = (server_rec *)params;
    
    /* Create the subpool for each connection
     * This keeps the memory consumption constant
     * when disconnecting from backend.
     */
    apr_pool_create(&ctx, pool);
    conn = apr_pcalloc(ctx, sizeof(proxy_conn_rec));

    conn->pool = ctx;
    *resource = conn;
    /* register the pool cleanup */
    apr_pool_cleanup_register(ctx, (void *)conn,
                              proxy_conn_cleanup,
                              apr_pool_cleanup_null);      

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "proxy: socket is constructed");

    return APR_SUCCESS;
}

/* reslist destructor */
static apr_status_t connection_destructor(void *resource, void *params,
                                          apr_pool_t *pool)
{
    proxy_conn_rec *conn = (proxy_conn_rec *)resource;
    server_rec *s = (server_rec *)params;
    
#if 0
    if (conn->sock)
        apr_socket_close(conn->sock);
    conn->sock = NULL;
    apr_pool_cleanup_kill(conn->pool, conn, proxy_conn_cleanup);
#endif
    if (conn->pool)
        apr_pool_destroy(conn->pool);
    conn->pool = NULL;
#if 0
    if (s != NULL)
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "proxy: socket is destructed");
#endif
    return APR_SUCCESS;
}

/* Close the connection 
 * The proxy_conn_rec from now on can not be used
 */
PROXY_DECLARE(apr_status_t) ap_proxy_close_connection(proxy_conn_rec *conn)
{

    if (conn->worker && conn->worker->cp)
        conn->worker->cp->conn = NULL;
    return connection_destructor(conn, NULL, NULL);
}

static apr_status_t init_conn_worker(proxy_worker *worker, server_rec *s)
{
    apr_status_t rv;

#if APR_HAS_THREADS
    int mpm_threads;

    ap_mpm_query(AP_MPMQ_MAX_THREADS, &mpm_threads);
    if (mpm_threads > 1) {
        /* Set hard max to no more then mpm_threads */
        if (worker->hmax == 0 || worker->hmax > mpm_threads)
            worker->hmax = mpm_threads;
        if (worker->smax == 0 || worker->smax > worker->hmax)
            worker->smax = worker->hmax;
        /* Set min to be lower then smax */
        if (worker->min > worker->smax)
            worker->min = worker->smax; 
        worker->cp->nfree = worker->hmax;
    }
    else {
        /* This will supress the apr_reslist creation */
        worker->min = worker->smax = worker->hmax = 0;
    }
    if (worker->hmax) {
        rv = apr_reslist_create(&(worker->cp->res),
                                worker->min, worker->smax,
                                worker->hmax, worker->ttl,
                                connection_constructor, connection_destructor,
                                s, worker->cp->pool);
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "proxy: initialized worker for (%s) min=%d max=%d smax=%d",
                      worker->hostname, worker->min, worker->hmax, worker->smax);

#if (APR_MAJOR_VERSION > 0)
        /* Set the acquire timeout */
        if (rv == APR_SUCCESS && worker->acquire_set)
            apr_reslist_timeout_set(worker->cp->res, worker->acquire);
#endif
    }
    else
#endif
    {
        
        connection_constructor((void **)&(worker->cp->conn), s, worker->cp->pool);
        rv = APR_SUCCESS;
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "proxy: initialized single connection worker for (%s)",
                      worker->hostname);
    }

    return rv;
}

PROXY_DECLARE(int) ap_proxy_retry_worker(const char *proxy_function,
                                         proxy_worker *worker,
                                         server_rec *s)
{
    if (worker->status & PROXY_WORKER_IN_ERROR) {
        apr_interval_time_t diff;
        apr_time_t now = apr_time_now();
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                    "proxy: %s: retrying the worker for (%s)",
                     proxy_function, worker->hostname);
        if (worker->retry)
            diff = worker->retry;
        else
            diff = apr_time_from_sec((60 + 60 * worker->retries++));
        if (now > worker->error_time + diff) {
            worker->status &= ~PROXY_WORKER_IN_ERROR;
            ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                         "proxy: %s: worker for (%s) has been marked for retry",
                         proxy_function, worker->hostname);
            return OK;
        }
        else
            return DECLINED;
    }
    else
        return OK;
}

PROXY_DECLARE(int) ap_proxy_acquire_connection(const char *proxy_function,
                                               proxy_conn_rec **conn,
                                               proxy_worker *worker,
                                               server_rec *s)
{
    apr_status_t rv;

    if (!(worker->status & PROXY_WORKER_INITIALIZED)) {
        if ((rv = init_conn_worker(worker, s)) != APR_SUCCESS) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                         "proxy: %s: failed to initialize worker for (%s)",
                         proxy_function, worker->hostname);
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        worker->status = PROXY_WORKER_INITIALIZED;
    }

    if (!PROXY_WORKER_IS_USABLE(worker)) {
        /* Retry the worker */
        ap_proxy_retry_worker(proxy_function, worker, s);
    
        if (!PROXY_WORKER_IS_USABLE(worker)) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                         "proxy: %s: disabled connection for (%s)",
                         proxy_function, worker->hostname);
            return HTTP_SERVICE_UNAVAILABLE;
        }
    }
#if APR_HAS_THREADS
    if (worker->hmax) {
        rv = apr_reslist_acquire(worker->cp->res, (void **)conn);
    }
    else
#endif
    {
        /* create the new connection if the previous was destroyed */
        if (!worker->cp->conn)
            connection_constructor((void **)conn, s, worker->cp->pool);
        else {
            *conn = worker->cp->conn;
            worker->cp->conn = NULL;
        }
        rv = APR_SUCCESS;
    }

    if (rv != APR_SUCCESS) {
        ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                     "proxy: %s: failed to acquire connection for (%s)",
                     proxy_function, worker->hostname);
        return HTTP_SERVICE_UNAVAILABLE;
    }
    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "proxy: %s: has acquired connection for (%s)",
                 proxy_function, worker->hostname);

    (*conn)->worker = worker;

    return OK;
}

PROXY_DECLARE(int) ap_proxy_release_connection(const char *proxy_function,
                                               proxy_conn_rec *conn,
                                               server_rec *s)
{
    apr_status_t rv = APR_SUCCESS;

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "proxy: %s: has relesed connection for (%s)",
                 proxy_function, conn->worker->hostname);
    /* If there is a connection kill it's cleanup */
    if (conn->connection)
        apr_pool_cleanup_kill(conn->connection->pool, conn, connection_cleanup);
    connection_cleanup(conn);
    conn->connection = NULL;

    return OK;
}

PROXY_DECLARE(int)
ap_proxy_determine_connection(apr_pool_t *p, request_rec *r,
                              proxy_server_conf *conf,
                              proxy_worker *worker,
                              proxy_conn_rec *conn,
                              apr_pool_t *ppool,
                              apr_uri_t *uri,
                              char **url,
                              const char *proxyname,
                              apr_port_t proxyport,
                              char *server_portstr,
                              int server_portstr_size)
{
    int server_port;
    apr_status_t err = APR_SUCCESS;
    /*
     * Break up the URL to determine the host to connect to
     */

    /* we break the URL into host, port, uri */
    if (APR_SUCCESS != apr_uri_parse(p, *url, uri)) {
        return ap_proxyerror(r, HTTP_BAD_REQUEST,
                             apr_pstrcat(p,"URI cannot be parsed: ", *url,
                                         NULL));
    }
    if (!uri->port) {
        uri->port = apr_uri_port_of_scheme(uri->scheme);
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, r->server,
                 "proxy: connecting %s to %s:%d", *url, uri->hostname,
                 uri->port);

    /* allocate these out of the specified connection pool 
     * The scheme handler decides if this is permanent or
     * short living pool.
     */
    /* are we connecting directly, or via a proxy? */
    if (proxyname) {
        conn->hostname = apr_pstrdup(ppool, proxyname);
        conn->port = proxyport;
    } else {
        conn->hostname = apr_pstrdup(ppool, uri->hostname);
        conn->port = uri->port;
        *url = apr_pstrcat(p, uri->path, uri->query ? "?" : "",
                           uri->query ? uri->query : "",
                           uri->fragment ? "#" : "",
                           uri->fragment ? uri->fragment : "", NULL);
    }
    /* Worker can have the single constant backend adress.
     * The single DNS lookup is used once per worker.
     * If dynamic change is needed then set the addr to NULL
     * inside dynamic config to force the lookup.
     */
    if (!worker->cp->addr)
        err = apr_sockaddr_info_get(&(worker->cp->addr),
                                    conn->hostname, APR_UNSPEC,
                                    conn->port, 0,
                                    worker->cp->pool);

    if (err != APR_SUCCESS) {
        return ap_proxyerror(r, HTTP_BAD_GATEWAY,
                             apr_pstrcat(p, "DNS lookup failure for: ",
                                         conn->hostname, NULL));
    }

    /* Get the server port for the Via headers */
    {
        server_port = ap_get_server_port(r);
        if (ap_is_default_port(server_port, r)) {
            strcpy(server_portstr,"");
        } else {
            apr_snprintf(server_portstr, server_portstr_size, ":%d",
                         server_port);
        }
    }

    /* check if ProxyBlock directive on this host */
    if (OK != ap_proxy_checkproxyblock(r, conf, worker->cp->addr)) {
        return ap_proxyerror(r, HTTP_FORBIDDEN,
                             "Connect to remote machine blocked");
    }
    return OK;
}

static int is_socket_connected(apr_socket_t *sock)

{
    apr_size_t buffer_len = 1;
    char test_buffer[1]; 
    apr_status_t socket_status;
    apr_interval_time_t current_timeout;
    
    /* save timeout */
    apr_socket_timeout_get(sock, &current_timeout);
    /* set no timeout */
    apr_socket_timeout_set(sock, 0);
    socket_status = apr_socket_recv(sock, test_buffer, &buffer_len);
    /* put back old timeout */
    apr_socket_timeout_set(sock, current_timeout);
    if (APR_STATUS_IS_EOF(socket_status))
        return 0;
    else
        return 1;
}

PROXY_DECLARE(int) ap_proxy_connect_backend(const char *proxy_function,
                                            proxy_conn_rec *conn,
                                            proxy_worker *worker,
                                            server_rec *s)
{
    apr_status_t rv;
    int connected = 0;
    int loglevel;
    apr_sockaddr_t *backend_addr = worker->cp->addr;
    apr_socket_t *newsock;
    
    if (conn->sock) {
        /* This increases the connection pool size
         * but the number of dropped connections is
         * relatively small compared to connection lifetime
         */
        if (!(connected = is_socket_connected(conn->sock))) {        
            apr_socket_close(conn->sock);
            conn->sock = NULL;
        }
    }
    while (backend_addr && !connected) {
        if ((rv = apr_socket_create(&newsock, backend_addr->family,
                                SOCK_STREAM, APR_PROTO_TCP,
                                conn->pool)) != APR_SUCCESS) {
            loglevel = backend_addr->next ? APLOG_DEBUG : APLOG_ERR;
            ap_log_error(APLOG_MARK, loglevel, rv, s,
                         "proxy: %s: error creating fam %d socket for target %s",
                         proxy_function,
                         backend_addr->family,
                         worker->hostname);
            /* this could be an IPv6 address from the DNS but the
             * local machine won't give us an IPv6 socket; hopefully the
             * DNS returned an additional address to try
             */
            backend_addr = backend_addr->next;
            continue;
        }

#if !defined(TPF) && !defined(BEOS)
        if (worker->recv_buffer_size > 0 &&
            (rv = apr_socket_opt_set(newsock, APR_SO_RCVBUF,
                                     worker->recv_buffer_size))) {
            ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                         "apr_socket_opt_set(SO_RCVBUF): Failed to set "
                         "ProxyReceiveBufferSize, using default");
        }
#endif

        /* Set a timeout on the socket */
        if (worker->timeout_set == 1) {
            apr_socket_timeout_set(newsock, worker->timeout);
        }
        else {
             apr_socket_timeout_set(newsock, s->timeout);
        }
        /* Set a keepalive option */
        if (worker->keepalive) {
            if ((rv = apr_socket_opt_set(newsock, 
                            APR_SO_KEEPALIVE, 1)) != APR_SUCCESS) {
                ap_log_error(APLOG_MARK, APLOG_ERR, rv, s,
                             "apr_socket_opt_set(SO_KEEPALIVE): Failed to set"
                             " Keepalive");
            }
        }
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                     "proxy: %s: fam %d socket created to connect to %s",
                     proxy_function, backend_addr->family, worker->hostname);

        /* make the connection out of the socket */
        rv = apr_socket_connect(newsock, backend_addr);

        /* if an error occurred, loop round and try again */
        if (rv != APR_SUCCESS) {
            apr_socket_close(newsock);
            loglevel = backend_addr->next ? APLOG_DEBUG : APLOG_ERR;
            ap_log_error(APLOG_MARK, loglevel, rv, s,
                         "proxy: %s: attempt to connect to %pI (%s) failed",
                         proxy_function,
                         backend_addr,
                         worker->hostname);
            backend_addr = backend_addr->next;
            continue;
        }
        
        conn->sock   = newsock;
        connected    = 1;
    }
    /* Put the entire worker to error state
     * Altrough some connections may be alive
     * no further connections to the worker could be made
     */
    if (!connected && PROXY_WORKER_IS_USABLE(worker)) {
        worker->status |= PROXY_WORKER_IN_ERROR;
        worker->error_time = apr_time_now();
        ap_log_error(APLOG_MARK, APLOG_ERR, 0, s,
            "ap_proxy_connect_backend disabling worker for (%s)",
            worker->hostname);
    }
    else {
        worker->error_time = 0;
        worker->retries = 0;
    }
    return connected ? OK : DECLINED;
}

PROXY_DECLARE(int) ap_proxy_connection_create(const char *proxy_function,
                                              proxy_conn_rec *conn,
                                              conn_rec *c,
                                              server_rec *s)
{
    proxy_worker *worker = conn->worker;
    apr_sockaddr_t *backend_addr = worker->cp->addr;

    /* The socket is now open, create a new backend server connection 
    * 
    */
    conn->connection = ap_run_create_connection(c->pool, s, conn->sock,
                                                c->id, c->sbh,
                                                c->bucket_alloc);

    if (!conn->connection) {
        /* the peer reset the connection already; ap_run_create_connection() 
        * closed the socket
        */
        ap_log_error(APLOG_MARK, APLOG_DEBUG, 0,
                     s, "proxy: %s: an error occurred creating a "
                     "new connection to %pI (%s)", proxy_function,
                     backend_addr, conn->hostname);
        /* XXX: Will be closed when proxy_conn is closed */
        apr_socket_close(conn->sock);
        conn->sock = NULL;
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    /* register the connection cleanup to client connection
     * so that the connection can be closed or reused
     */
    apr_pool_cleanup_register(c->pool, (void *)conn,
                              connection_cleanup,
                              apr_pool_cleanup_null);      

    /* For ssl connection to backend */
    if (conn->is_ssl) {
        if (!ap_proxy_ssl_enable(conn->connection)) {
            ap_log_error(APLOG_MARK, APLOG_ERR, 0,
                         s, "proxy: %s: failed to enable ssl support "
                         "for %pI (%s)", proxy_function, 
                         backend_addr, conn->hostname);
            return HTTP_INTERNAL_SERVER_ERROR;
        }
    }
    else {
        /* TODO: See if this will break FTP */
        ap_proxy_ssl_disable(conn->connection);
    }

    ap_log_error(APLOG_MARK, APLOG_DEBUG, 0, s,
                 "proxy: %s: connection complete to %pI (%s)",
                 proxy_function, backend_addr, conn->hostname);

    /* set up the connection filters */
    ap_run_pre_connection(conn->connection, conn->sock);

    return OK;
}

PROXY_DECLARE(int) ap_proxy_lb_workers(void)
{
    /* Set the dynamic #workers limit */
    lb_workers_limit = lb_workers + PROXY_DYNAMIC_BALANCER_LIMIT;
    return lb_workers_limit;
}
