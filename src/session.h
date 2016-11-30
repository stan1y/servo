#ifndef _SERVO_SESSION_H_
#define _SERVO_SESSION_H_

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>
#include <jansson.h>

struct servo_session {
    union {
	struct sockaddr_in  ipv4;
	struct sockaddr_in6 ipv6;
    } addr;
    int addrtype;

    char *client_name;

    time_t ttl;
    struct tm expire_at;


    TAILQ_ENTRY(servo_session) list;
};

TAILQ_HEAD(servo_session_head, servo_session);

/**
 * Get or create session for this client request
 */
struct servo_session *
servo_get_session(struct http_request *);

/**
 * Get client name
 */
const char *
servo_get_client_ipaddr(struct servo_session *s);

#endif //_SERVO_SESSION_H_
