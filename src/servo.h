#ifndef _SERVO_H_
#define _SERVO_H_

#include <kore/kore.h>
#include <kore/http.h>
#include <jansson.h>

#include "storage.h"
#include "session.h"

/**
 * Servo context
 */
struct servo_context {
    struct servo_session_head sessions;
    unsigned long session_ttl;
};

/**
 * Alloc and initialize new servo context
 */
struct servo_context *
servo_init(void);


/**
 * Get or create new session based on current settings
 * May raise 403 error if requetor is forbidden for any reason
 */
int servo_session(struct http_request *);


/**
 * Trigger event
 */
int servo_event(struct http_request *);

#endif //_SERVO_H_
