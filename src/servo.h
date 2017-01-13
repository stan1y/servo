#ifndef _SERVO_H_
#define _SERVO_H_

#include <uuid/uuid.h>

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>
#include <jansson.h>

#include "util.h"

#define CLIENT_LEN 255
#define ITEM_KEY_LEN 255

struct servo_session {
    char client[CLIENT_LEN];
    time_t expire_on;
};

struct servo_config {
	// connection string
	char *connect;

	// enable Public Mode
	int public_mode;
	
	// global session TTL
	size_t session_ttl;

	// total number of stored sessions
	size_t max_sessions;

	// filter by Origin header
	char *allow_origin;

	// filter by Connection's ip address
	char *allow_ipaddr;
};

struct servo_context {
    struct kore_pgsql	 sql;
    struct servo_session session;

    /* IN and OUT options of Content-Type */
    int in_content_type;
    int out_content_type;

    /* Requested item data */
    char	*str_val;
    json_t	*json_val;
    void	*blob_val;
};

struct servo_context * servo_create_context(struct http_request *req);
int servo_put_session(struct servo_session *s);
int servo_read_config(struct servo_config *cfg);

/**
 * Bootstrap servo service on start
 */
int servo_init(int state);

/**
 * Sessions API entry point.
 * - session index
 * - session item get/post/put/delete
 */
int servo_start(struct http_request *);

/* Render JSON stats to API clients */
int servo_render_stats(struct http_request *req);

/* Render Debug console */
int servo_render_console(struct http_request *req);

#endif //_SERVO_H_
