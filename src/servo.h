#ifndef _SERVO_H_
#define _SERVO_H_

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>

#include <uuid/uuid.h>
#include <jwt/jwt.h>
#include <jansson.h>

/* States */

#define REQ_STATE_INIT          0
#define REQ_STATE_QUERY         1
#define REQ_STATE_WAIT          2
#define REQ_STATE_READ          3
#define REQ_STATE_ERROR         4
#define REQ_STATE_DONE          5

/* Common */

#define CLIENT_UUID_LEN         37
#define ITEM_KEY_MAX            255

#define PGSQL_FORMAT_TEXT       0
#define PGSQL_FORMAT_BINARY     1

#define CONSOLE_JS_PATH         "/console.js"
#define ROOT_PATH               "/"
#define AUTH_HEADER             "authorization"
#define AUTH_TYPE_PREFIX        "Bearer "

static char* DBNAME = "servo-store";

static char    *SQL_STATE_NAMES[] = {
    "<null>",   // NULL
    "init",     // KORE_PGSQL_STATE_INIT
    "wait",     // KORE_PGSQL_STATE_WAIT
    "result",   // KORE_PGSQL_STATE_RESULT
    "error",    // KORE_PGSQL_STATE_ERROR
    "done",     // KORE_PGSQL_STATE_DONE
    "complete"  // KORE_PGSQL_STATE_COMPLETE
};

static char    *SERVO_CONTENT_NAMES[] = {
    "string",
    "json",
    "binary"
};

struct servo_config {

    char        *database;
    int          public_mode;
    size_t       session_ttl;
    size_t       max_sessions;
    char        *jwt_key;
    size_t       jwt_key_len;

    /* values size limits */
    size_t       string_size;
    size_t       json_size;
    size_t       blob_size;

    /* filtering */
    char         *allow_origin;
    char         *allow_ipaddr;
};

/* shared config instance */
extern struct servo_config *CONFIG;

struct servo_context {
    // processing status 
    // & err message
    int                  status;
    char                *err;

    // PgSQL engine
    struct kore_pgsql    sql;

    // Client ID and web token
    char                *client;
    jwt_t               *token;

    // in/out content-type
    int                  in_content_type;
    int                  out_content_type;

    // Current item data
    char                *val_str;
    json_t              *val_json;
    void                *val_blob;
    size_t               val_sz;
};

struct servo_context    *servo_create_context(struct http_request *);
int                      servo_init_context(struct servo_context *);
int                      servo_read_context_token(struct http_request *);
void                     servo_write_context_token(struct http_request *);
void                     servo_clear_context(struct servo_context *);
int                      servo_put_context(struct servo_context *);
int                      servo_purge_context(struct servo_context *);

int                      servo_init(int state);
int                      servo_start(struct http_request *);
int                      servo_render_stats(struct http_request *);
int                      servo_render_console(struct http_request *);
int                      servo_render_console_js(struct http_request *);

int                      servo_state_init(struct http_request *);
int                      servo_state_query(struct http_request *);
int                      servo_state_wait(struct http_request *);
int                      servo_state_read(struct http_request *);
int                      state_error(struct http_request *);
int                      state_done(struct http_request *);

int                      state_handle_get(struct http_request *);
int                      state_handle_post(struct http_request *, struct kore_buf *);
int                      state_handle_put(struct http_request *, struct kore_buf *);
int                      state_handle_delete(struct http_request *);
int                      state_handle_head(struct http_request *);


#endif //_SERVO_H_
