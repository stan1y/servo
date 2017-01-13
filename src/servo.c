#include "servo.h"
#include "util.h"
#include "assets.h"

/**
 * Session API states
 */ 
static int	state_connect_session(struct http_request *);
static int	state_query_session(struct http_request *);
static int  state_wait_session(struct http_request *);
static int  state_read_session(struct http_request *);

static int  state_connect_item(struct http_request *);
static int	state_query_item(struct http_request *);
static int	state_wait_item(struct http_request *);
static int  state_read_item(struct http_request *);
static int	state_error(struct http_request *);
static int	state_done(struct http_request *);

static int  servo_connect_db(struct http_request *, int, int, int);
static int  servo_wait(struct http_request *, int, int, int);

static struct servo_config *CONFIG;

#define REQ_STATE_C_SESSION     0
#define REQ_STATE_Q_SESSION     1
#define REQ_STATE_W_SESSION     2
#define REQ_STATE_R_SESSION     3
#define REQ_STATE_C_ITEM        4
#define REQ_STATE_Q_ITEM        5
#define REQ_STATE_W_ITEM        6
#define REQ_STATE_R_ITEM        7
#define REQ_STATE_ERROR         8
#define REQ_STATE_DONE          9

struct http_state   servo_session_states[] = {
    { "REQ_STATE_C_SESSION",  state_connect_session },
    { "REQ_STATE_Q_SESSION",  state_query_session },
    { "REQ_STATE_W_SESSION",  state_wait_session },
    { "REQ_STATE_R_SESSION",  state_read_session },

    { "REQ_STATE_C_ITEM",     state_connect_item },
    { "REQ_STATE_Q_ITEM",	  state_query_item },
    { "REQ_STATE_W_ITEM",	  state_wait_item },
    { "REQ_STATE_R_ITEM",     state_read_item },

    { "REQ_STATE_ERROR",      state_error },
    { "REQ_STATE_DONE",		  state_done },
};

#define servo_session_states_size (sizeof(servo_session_states) / sizeof(servo_session_states[0]))

/* PGSQL result formats */
#define PGSQL_FORMAT_TEXT 0
#define PGSQL_FORMAT_BINARY 1

/**
 * Create new servo_context
 */ 
struct servo_context * servo_create_context(struct http_request *req)
{
    struct servo_context *ctx;

    ctx = kore_malloc(sizeof(struct servo_context));
    memset(ctx->session.client, 0, sizeof(ctx->session.client));

    /* read and write strings by default */
    ctx->in_content_type = SERVO_CONTENT_STRING;
    ctx->out_content_type = SERVO_CONTENT_STRING;

    return ctx;
}

/**
 * Write session information to db (sync)
 */
int servo_put_session(struct servo_session *s)
{
    struct kore_pgsql   sql;
    char                sexpire_on[BUFSIZ];

    if (!kore_pgsql_query_init(&sql, NULL, "db", KORE_PGSQL_SYNC)) {
        kore_log(LOG_ERR, "%s: failed to init query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    memset(sexpire_on, 0, BUFSIZ);
    snprintf(sexpire_on, BUFSIZ, "%ju", s->expire_on);
    if (!kore_pgsql_query_params(&sql, 
                                 (const char*)asset_put_session_sql,
                                 PGSQL_FORMAT_TEXT,
                                 2,
                                 s->client, strlen(s->client), PGSQL_FORMAT_TEXT,
                                 sexpire_on, strlen(s->client), PGSQL_FORMAT_TEXT)) {
        kore_log(LOG_ERR, "%s: failed to run query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    kore_pgsql_cleanup(&sql);
    kore_log(LOG_NOTICE, "created session %s", s->client);
    return (KORE_RESULT_OK);
}


/**
 * Bootstap servo
 */
int servo_init(int state)
{
    CONFIG = kore_malloc(sizeof(struct servo_config));
    memset(CONFIG, 0, sizeof(struct servo_config));

    /* Configuration defaults */
    CONFIG->public_mode = 0;
    CONFIG->session_ttl = 300;
    CONFIG->max_sessions = 10;
    CONFIG->allow_origin = NULL;
    CONFIG->allow_ipaddr = NULL;

    if (!servo_read_config(CONFIG)) {
        kore_log(LOG_ERR, "%s: failed to read config", __FUNCTION__);
        return (KORE_RESULT_ERROR);
    }

    kore_log(LOG_NOTICE, "initializing pid: %d", (int)getpid());
    kore_log(LOG_NOTICE, "public mode: %s", CONFIG->public_mode != 0 ? "yes" : "no");
    kore_log(LOG_NOTICE, "session ttl: %d seconds", CONFIG->session_ttl);
    kore_log(LOG_NOTICE, "max sessions: %d", CONFIG->max_sessions);
    if (CONFIG->allow_origin != NULL)
        kore_log(LOG_NOTICE, "allow origin: %s", CONFIG->allow_origin);
    if (CONFIG->allow_ipaddr != NULL)
        kore_log(LOG_NOTICE, "allow ip address: %s", CONFIG->allow_ipaddr);
    
    kore_pgsql_register("db", CONFIG->connect);
    
    return (KORE_RESULT_OK);
}


/**
 * Servo Session API entry point
 */
int servo_start(struct http_request *req)
{
    char        *origin;
    char         saddr[INET6_ADDRSTRLEN];

    /* Filter by Origin header */
    if (strlen(req->path) > 1 && CONFIG->allow_origin != NULL) {
        if (!http_request_header(req, "Origin", &origin) && !CONFIG->public_mode) {
            kore_log(LOG_NOTICE, "%s: disallow access - no 'Origin' header sent",
                __FUNCTION__);
            servo_response_error(req, 403, "'Origin' header is not found");
            return (KORE_RESULT_OK);
        }
        if (strcmp(origin, CONFIG->allow_origin) != 0) {
            kore_log(LOG_NOTICE, "%s: disallow access - 'Origin' header mismatch %s != %s",
                __FUNCTION__,
                origin, CONFIG->allow_origin);
            servo_response_error(req, 403, "Origin Access Denied");
            return (KORE_RESULT_OK);
        }
    }

    /* Filter by client ip address */
    if (CONFIG->allow_ipaddr != NULL) {
        memset(saddr, 0, sizeof(saddr));
        if (req->owner->addrtype == AF_INET) {
            inet_ntop(AF_INET, &req->owner->addr.ipv4.sin_addr, saddr, sizeof(saddr));
        }
        if (req->owner->addrtype == AF_INET6) {
            inet_ntop(AF_INET6, &req->owner->addr.ipv6.sin6_addr, saddr, sizeof(saddr));
        }

        if (strcmp(saddr, CONFIG->allow_ipaddr) != 0) {
            kore_log(LOG_NOTICE, "%s: disallow access - Client IP mismatch %s != %s",
                __FUNCTION__, saddr, CONFIG->allow_ipaddr);
            servo_response_error(req, 403, "Client Access Denied");
            return (KORE_RESULT_OK);
        }
    }

    /* Initialize request context and run */
    if (req->hdlr_extra == NULL) {
       req->hdlr_extra = servo_create_context(req);
    }
    return (http_state_run(servo_session_states, servo_session_states_size, req));
}

int
servo_connect_db(struct http_request *req, int retry_step, int success_step, int error_step)
{
    struct servo_context *ctx = req->hdlr_extra;

    if (!kore_pgsql_query_init(&ctx->sql, req, "db", KORE_PGSQL_ASYNC)) {

        /* If the state was still INIT, we'll try again later. */
        if (ctx->sql.state == KORE_PGSQL_STATE_INIT) {
            req->fsm_state = retry_step;
            kore_log(LOG_DEBUG, "retrying db connection...");
            return (HTTP_STATE_RETRY);
        }

        /* Different state means error */
        kore_pgsql_logerror(&ctx->sql);
        req->fsm_state = error_step;
    }
    else {
        req->fsm_state = success_step;
        kore_log(LOG_DEBUG, "connected to db -> (%d)",
            req->fsm_state);
    }

    return (HTTP_STATE_CONTINUE);
}

int
state_connect_session(struct http_request *req)
{
    return servo_connect_db(req, 
                            REQ_STATE_C_SESSION,
                            REQ_STATE_Q_SESSION,
                            REQ_STATE_ERROR);
}

int
state_query_session(struct http_request *req)
{
    int                      rc = KORE_RESULT_OK;
    struct servo_context    *ctx = req->hdlr_extra;
    char                    *usrclient;
    uuid_t                   cid;

    if (!ctx->session.client || strlen(ctx->session.client) == 0) {
        /* Check request header first */
        if (http_request_header(req, "X-Servo-Client", &usrclient)) {
            strncpy(ctx->session.client, usrclient, sizeof(ctx->session.client) - 1);
        }
    }

    if (!ctx->session.client || strlen(ctx->session.client) == 0) {
        /* Read cookie next */
        http_populate_cookies(req);
        if (http_request_cookie(req, "Servo-Client", &usrclient)) {
            strncpy(ctx->session.client, usrclient, sizeof(ctx->session.client) - 1);
        }
    }

    if (!ctx->session.client || strlen(ctx->session.client) == 0) {
        /* Generate new client id and init fresh session */
        uuid_generate(cid);
        memset(ctx->session.client, 9, sizeof(ctx->session.client));
        uuid_unparse(cid, ctx->session.client);
        ctx->session.expire_on = time(NULL) + CONFIG->session_ttl;
        
        /* Proceed to REQ_STATE_C_ITEM with newly created session */
        req->fsm_state = REQ_STATE_C_ITEM;
        if (!servo_put_session(&ctx->session)) {
            req->fsm_state = REQ_STATE_ERROR;
        }
    }
    else {

        /* Client reported ID, so Servo lookups existing session 
         * 'asset_query_session_sql' - $1 client id
         */

        rc = kore_pgsql_query_params(&ctx->sql, 
                                    (const char*)asset_query_session_sql,
                                    PGSQL_FORMAT_TEXT,
                                    1,
                                    ctx->session.client,
                                    strlen(ctx->session.client),
                                    PGSQL_FORMAT_TEXT);
        /* Wait for session data in REQ_STATE_W_SESSION or report error */
        req->fsm_state = REQ_STATE_W_SESSION;
        if (rc != KORE_RESULT_OK) {
            kore_log(LOG_ERR, "Failed to execute query.");
            req->fsm_state = REQ_STATE_ERROR;
        }
    }
    /* now we know client id and have a session, 
       make sure requestor knows it too if case we 
       had generated a new anonymous session */
    http_response_cookie(req, "Servo-Client", ctx->session.client);
    http_response_header(req, "X-Servo-Client", ctx->session.client);
    kore_log(LOG_NOTICE, "serving client {%s}", ctx->session.client);

    /* check if item was requested or index */
    if (strcmp(req->path, "/") == 0) {
        if (CONFIG->public_mode)
            rc = servo_render_console(req);
        else
            rc = servo_render_stats(req);

        if (rc == KORE_RESULT_ERROR) {
            /* report error in error state */
            req->fsm_state = REQ_STATE_ERROR;
        }
        else {
            /* complete request, response was rendered */
            req->fsm_state = REQ_STATE_DONE;
        }
    }

    /* State machine */
    return HTTP_STATE_CONTINUE; 
}

int
servo_wait(struct http_request *req, int read_step, int complete_step, int error_step)
{
    struct servo_context *ctx = req->hdlr_extra;

    switch (ctx->sql.state) {
    case KORE_PGSQL_STATE_WAIT:
        /* keep waiting */
        kore_log(LOG_DEBUG, "wating...");
        return (HTTP_STATE_RETRY);

    case KORE_PGSQL_STATE_COMPLETE:
        kore_log(LOG_DEBUG, "complete -> (%d)", complete_step);
        req->fsm_state = complete_step;
        break;

    case KORE_PGSQL_STATE_RESULT:
        kore_log(LOG_DEBUG, "reading... -> (%d)", read_step);
        req->fsm_state = read_step;
        break;

    case KORE_PGSQL_STATE_ERROR:
        req->fsm_state = error_step;
        kore_pgsql_logerror(&ctx->sql);
        break;

    default:
        /* This MUST be present in order to advance the pgsql state */
        kore_pgsql_continue(req, &ctx->sql);
        break;
    }

    return (HTTP_STATE_CONTINUE);
}

int
state_wait_session(struct http_request *req)
{
    return servo_wait(req, REQ_STATE_R_SESSION,
                           REQ_STATE_C_ITEM,
                           REQ_STATE_ERROR);
}

/* Handle session query data fetch */
int state_read_session(struct http_request *req)
{
    int                      rows;
    char                    *value;
    struct servo_context    *ctx;
    time_t                   now;


    rows = 0;
    now = time(NULL);
    ctx = (struct servo_context*)req->hdlr_extra;

    rows = kore_pgsql_ntuples(&ctx->sql);
    if (rows == 0) {
        /* known client but no session record, expired? */
        ctx->session.expire_on = time(NULL) + CONFIG->session_ttl;
        kore_log(LOG_NOTICE, "%s: session %s was not found",
            __FUNCTION__, ctx->session.client);
        if (!servo_put_session(&ctx->session)) {
            req->fsm_state = REQ_STATE_ERROR;
            return (HTTP_STATE_CONTINUE);
        }
    }
    else if (rows == 1) {
        /* found existing session record */
        value = kore_pgsql_getvalue(&ctx->sql, 0, 0);
        ctx->session.expire_on = kore_date_to_time(value);
        if (now >= ctx->session.expire_on) {
            kore_log(LOG_NOTICE, "expired session {%s}", ctx->session.client);
        }
        else 
            kore_log(LOG_NOTICE, "existing session {%s}, expires on: %s",
                            ctx->session.client,
                            value);
    }
    else {
        kore_log(LOG_ERR, "selected %d rows, 1 expected", rows);
        return (HTTP_STATE_ERROR);  
    }

    /* Continue processing our query results. */
    kore_pgsql_continue(req, &ctx->sql);

    /* Back to our DB waiting state. */
    req->fsm_state = REQ_STATE_W_SESSION;
    return (HTTP_STATE_CONTINUE);
}

int
state_connect_item(struct http_request *req)
{
    return servo_connect_db(req,
                            REQ_STATE_C_ITEM,
                            REQ_STATE_Q_ITEM,
                            REQ_STATE_ERROR);
}

int state_query_item(struct http_request *req)
{
    int                      rc;
    struct servo_context    *ctx;
    char                    *str_val;
    json_t                  *json_val;
    void                    *blob_val;

    rc = KORE_RESULT_OK;
    ctx = (struct servo_context*)req->hdlr_extra;

    switch(req->method) {
        case HTTP_METHOD_POST:
            /* post_item.sql expects 5 arguments: 
             client, key, string, json, blob
            */
            kore_log(LOG_NOTICE, "POST %s for {%s}",
                req->path, ctx->session.client);

            switch(ctx->in_content_type) {
                default:
                case SERVO_CONTENT_STRING:
                    str_val = servo_request_str_data(req);
                    rc = kore_pgsql_query_params(&ctx->sql, 
                                        (const char*)asset_post_item_sql, 
                                        PGSQL_FORMAT_TEXT,
                                        5,
                                        ctx->session.client,
                                        req->path,
                                        str_val,
                                        "NULL",
                                        "NULL");
                    break;

                case SERVO_CONTENT_JSON:
                    json_val = servo_request_json_data(req);
                    rc = kore_pgsql_query_params(&ctx->sql, 
                                        (const char*)asset_post_item_sql, 
                                        PGSQL_FORMAT_TEXT,
                                        5,
                                        ctx->session.client,
                                        req->path,
                                        "NULL",
                                        json_dumps(json_val, JSON_ENCODE_ANY),
                                        "NULL");
                    break;

                case SERVO_CONTENT_BLOB:
                    blob_val = (void *)servo_request_str_data(req);
                    rc = kore_pgsql_query_params(&ctx->sql, 
                                        (const char*)asset_post_item_sql, 
                                        PGSQL_FORMAT_TEXT,
                                        5,
                                        ctx->session.client,
                                        req->path,
                                        "NULL",
                                        "NULL",
                                        (const char*)blob_val);
                    break;
            } 
            break;

        case HTTP_METHOD_PUT:
            break;

        case HTTP_METHOD_DELETE:
            break;

        default:
        case HTTP_METHOD_GET:
            /* get_item.sql
             * $1 - client id
             * $2 - item key 
             */
            kore_log(LOG_NOTICE, "GET %s for {%s}",
                req->path, ctx->session.client);
            rc = kore_pgsql_query_params(&ctx->sql, 
                                        (const char*)asset_get_item_sql, 
                                        PGSQL_FORMAT_TEXT,
                                        2,
                                        ctx->session.client,
                                        strlen(ctx->session.client),
                                        PGSQL_FORMAT_TEXT,
                                        req->path,
                                        strlen(req->path),
                                        PGSQL_FORMAT_TEXT);
            break;
    }

    if (rc != KORE_RESULT_OK) {
        kore_pgsql_logerror(&ctx->sql);
        return HTTP_STATE_ERROR;
    }

    /* Wait for item request completition */
    req->fsm_state = REQ_STATE_W_ITEM;
    return HTTP_STATE_CONTINUE;
}

int state_wait_item(struct http_request *req)
{
    return servo_wait(req, REQ_STATE_R_ITEM,
                           REQ_STATE_DONE,
                           REQ_STATE_ERROR);
}

int state_read_item(struct http_request *req)
{
    int                      rows;               
    struct servo_context    *ctx;
    static char             *not_found = "Item not found";

    rows = 0;
    ctx = (struct servo_context*)req->hdlr_extra;

    rows = kore_pgsql_ntuples(&ctx->sql);
    if (rows == 0 && req->method == HTTP_METHOD_GET) {
        /* item was not found, report 404 */
        kore_log(LOG_NOTICE, "%s: no item found for key \"%s\"",
            __FUNCTION__, req->path);

        http_response(req, 404, not_found, strlen(not_found));
        return HTTP_STATE_COMPLETE;
    }
    else if (rows == 1) {
        /* found existing session record */
        ctx->str_val = kore_pgsql_getvalue(&ctx->sql, 0, 0);
        ctx->json_val = kore_pgsql_getvalue(&ctx->sql, 0, 1);
        ctx->blob_val = kore_pgsql_getvalue(&ctx->sql, 0, 2);
        kore_log(LOG_NOTICE, "%s: reading...");
    }
    else {
        kore_log(LOG_ERR, "selected %d rows, 1 expected", rows);
        return (HTTP_STATE_ERROR);  
    }

    /* Continue processing our query results. */
    kore_pgsql_continue(req, &ctx->sql);

    /* Back to our DB waiting state. */
    req->fsm_state = REQ_STATE_W_SESSION;
    return (HTTP_STATE_CONTINUE);
}

/* An error occurred. */
int state_error(struct http_request *req)
{
    struct servo_context    *ctx = req->hdlr_extra;

    kore_pgsql_cleanup(&ctx->sql);
    http_response_header(req, "content-type", "text/html");
    http_response(req, 500, asset_error_html,
                            asset_len_error_html);

    kore_log(LOG_DEBUG, "%s: request failed", 
        __FUNCTION__);
    return (HTTP_STATE_COMPLETE);
}

/* Request was completed successfully. */
int state_done(struct http_request *req)
{
    struct servo_context *ctx = req->hdlr_extra;

    kore_pgsql_cleanup(&ctx->sql);
    kore_log(LOG_DEBUG, "request for {%s} complete",
        ctx->session.client);
    return (HTTP_STATE_COMPLETE);
}