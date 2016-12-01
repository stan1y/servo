#include "session.h"
#include "servo.h"
#include "util.h"

int
servo_check_origin(const char *origin);

int
servo_check_origin(const char *origin)
{
    return (KORE_RESULT_OK);
}

struct servo_session *
servo_get_session(struct http_request *req)
{
    static char expire_on[80];
    char *origin = NULL, *client_header = NULL, *client = NULL;
    time_t now;
    struct servo_session *s = NULL;
    struct servo_context *ctx = NULL;

    if (!http_request_header(req, "Origin", &origin)) {
	kore_log(LOG_ERR, "session refused for no origin in request");
	return NULL;
    }
    if (!servo_check_origin(origin)) {
	kore_log(LOG_ERR, "session from origin %s is forbidden", origin);
	return NULL;
    }

    if (client == NULL && servo_read_cookie(req, "Servo-Client", &client)) {
	kore_log(LOG_NOTICE, "session for Servo-Client=%s", client);
    }

    if (client == NULL && http_request_header(req, "X-Servo-Client", &client_header)) {
	client = kore_strdup(client_header);
	kore_log(LOG_NOTICE, "session for X-Servo-Client=%s", client);
    }

    if (client == NULL) {
	kore_log(LOG_NOTICE, "creating new session");
	// no session found, create new
        ctx = req->hdlr_extra;
        s = kore_malloc(sizeof(struct servo_session));
	s->client = kore_strdup("new-session-id");
        s->ttl = ctx->session_ttl;
        time(&now);
        now += s->ttl;
        s->expire = *localtime(&now);
        TAILQ_INSERT_TAIL(&ctx->sessions, s, list);
    }
    else {
        TAILQ_FOREACH(s, &ctx->sessions, list) {
	    if (strcmp(s->client, client) == 0)
		break;
	}
	kore_free(client);
    }
    
    if (s != NULL) {
        strftime(expire_on, sizeof(expire_on), "%a %Y-%m-%d %H:%M:%S %Z", &s->expire);
	kore_log(LOG_NOTICE, "session for %s, ttl=%d, expire on: %s",
			     s->client, s->ttl, expire_on);
    }
    return s;
}


