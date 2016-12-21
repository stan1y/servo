#ifndef _SERVO_H_
#define _SERVO_H_

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>
#include <jansson.h>

#include "util.h"

#define CLIENT_LEN 255
#define ITEM_KEY_LEN 255

struct servo_session {
    char client[CLIENT_LEN];
    struct tm expire;
};

struct servo_config {
	// connection string
	char *connect;

	// enable Public Mode
	int public_mode;
	
	// global session TTL
	size_t session_ttl;

	// total max of active sesions
	size_t max_sessions;
	
	// raw string value size
	size_t val_string_size;
	
	// json value size
	size_t val_json_size;
	
	// blob value size in bytes
	size_t val_blob_size;
};

struct servo_context {
    struct kore_pgsql	 sql;
    struct servo_session session;

    // ID of the item we're handling
    char item[ITEM_KEY_LEN];
    // Content-Type of received item data
    int in_content_type;
    // Content-Type expected by client
    int out_content_type;
};


struct servo_context * servo_create_context(void);
void servo_init_session(struct servo_session *s);
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
int servo_render_stats(struct http_request *req, struct servo_context *);

/* Render Debug console */
int servo_render_console(struct http_request *req, struct servo_context *);

#endif //_SERVO_H_
