#ifndef _SERVO_H_
#define _SERVO_H_

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>

#include <uuid/uuid.h>
#include <jwt.h>
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
#define CONTENT_TYPE_HEADER     "content-type"
#define AUTH_TYPE_PREFIX        "Bearer "
#define CORS_ALLOWORIGIN_HEADER "access-control-allow-origin"
#define CORS_EXPOSE_HEADER      "access-control-expose-headers"
#define CORS_ALLOW_HEADER       "access-control-allow-headers"

#define CONTENT_TYPE_STRING     "text/plain"
#define CONTENT_TYPE_JSON       "application/json"
#define CONTENT_TYPE_FORMDATA   "multipart/form-data"
#define CONTENT_TYPE_BASE64     "application/base64"
#define CONTENT_TYPE_HTML       "text/html"

static char    *SERVO_CONTENT_NAMES[] = {
    CONTENT_TYPE_STRING,
    CONTENT_TYPE_JSON,
    CONTENT_TYPE_FORMDATA,
    CONTENT_TYPE_BASE64,
    CONTENT_TYPE_HTML
};

#define SERVO_CONTENT_STRING    0
#define SERVO_CONTENT_JSON      1
#define SERVO_CONTENT_FORMDATA  2
#define SERVO_CONTENT_BASE64    3
#define SERVO_CONTENT_HTML      4

struct servo_config {

    char        *database;
    int          public_mode;
    size_t       session_ttl;
    size_t       max_sessions;
    char        *jwt_key;
    size_t       jwt_key_len;
    jwt_alg_t    jwt_alg;

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
    void                *val_bin;
    size_t               val_sz;
};

int                      servo_init_context(struct servo_context *);
int                      servo_read_context_token(struct http_request *);
void                     servo_write_context_token(struct http_request *);
void                     servo_delete_context(struct http_request *);

int                      servo_put_context(struct servo_context *);
int                      servo_purge_context(struct servo_context *);

int                      servo_init(int state);
int                      servo_start(struct http_request *);
int                      servo_render_stats(struct http_request *);

int                      servo_state_init(struct http_request *);
int                      servo_state_query(struct http_request *);
int                      servo_state_wait(struct http_request *);
int                      servo_state_read(struct http_request *);
int                      state_error(struct http_request *);
int                      state_done(struct http_request *);

int                      state_handle_get(struct http_request *);
int                      state_handle_post(struct http_request *, struct kore_buf *, struct http_file *);
int                      state_handle_put(struct http_request *, struct kore_buf *, struct http_file *);
int                      state_handle_delete(struct http_request *);
int                      state_handle_head(struct http_request *);

#endif //_SERVO_H_
