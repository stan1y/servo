#ifndef _SERVO_SESSION_H_
#define _SERVO_SESSION_H_

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>
#include <jansson.h>

struct servo_session {
    char *client;

    time_t ttl;
    struct tm expire;


    TAILQ_ENTRY(servo_session) list;
};

TAILQ_HEAD(servo_session_head, servo_session);

/**
 * Get or create session for this client request
 */
struct servo_session *
servo_get_session(struct http_request *);

#endif //_SERVO_SESSION_H_
