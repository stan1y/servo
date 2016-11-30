#include "session.h"
#include "servo.h"

struct servo_session *
servo_get_session(struct http_request *req)
{
    static char expire_at[80];
    int cmp;
    time_t now;
    struct servo_session *s;
    struct servo_context *ctx;

    ctx = req->hdlr_extra;
    TAILQ_FOREACH(s, &ctx->sessions, list) {
	cmp = INT_MAX;
	if (s->addrtype == AF_INET) {
	    cmp = s->addr.ipv4.sin_addr.s_addr - req->owner->addr.ipv4.sin_addr.s_addr;
        }
	if (s->addrtype == AF_INET6) {
	    cmp = memcmp((char *) &(s->addr.ipv6.sin6_addr.s6_addr),
		         (char *) &(req->owner->addr.ipv6.sin6_addr.s6_addr),
	                 sizeof(s->addr.ipv6.sin6_addr));
	}

	if (cmp == 0) {
	    return s;
	}
    }
    
    // no session found, create new
    s = kore_malloc(sizeof(struct servo_session));
    s->addrtype = req->owner->addrtype;
    switch (s->addrtype) {
	default:
	    kore_log(LOG_ERR, "%s: unsupported client familty type: %d",
		     __FUNCTION__, s->addrtype);
	    return NULL;
	    break;
	case AF_INET:
	    s->addr.ipv4 = req->owner->addr.ipv4;
	    break;
	case AF_INET6:
	    s->addr.ipv6 = req->owner->addr.ipv6;
	    break;
    }
    s->ttl = ctx->session_ttl;
    time(&now);
    now += s->ttl;
    s->expire_at = *localtime(&now);
    TAILQ_INSERT_TAIL(&ctx->sessions, s, list);
    
    strftime(expire_at, sizeof(expire_at), "%a %Y-%m-%d %H:%M:%S %Z", &s->expire_at);
    kore_log(LOG_NOTICE, "new session for %s, ttl=%d, expire on: %s", expire_at);

    return s;
}

const char *
servo_get_client_ipaddr(struct servo_session *s)
{
    static char ipaddr[INET_ADDRSTRLEN];
    memset(ipaddr, 0, sizeof(char) * INET_ADDRSTRLEN);

    switch (s->addrtype) {
    case AF_INET:
        /* IP is under connection->addr.ipv4 */
        inet_ntop(AF_INET, &(s->addr.ipv4), ipaddr, INET_ADDRSTRLEN);
        break;
    case AF_INET6:
        /* IP is under connection->addr.ipv6 */
        inet_ntop(AF_INET6, &(s->addr.ipv6), ipaddr, INET_ADDRSTRLEN);
        break;
    }

    return ipaddr;
}

