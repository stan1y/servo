#include "servo.h"
#include "util.h"
#include "assets.h"

/**
 * Session API states
 */ 
static int	state_connect(struct http_request *);
static int	state_query_session(struct http_request *);
static int  state_wait_session(struct http_request *);
static int  state_read_session(struct http_request *);
static int	state_query_item(struct http_request *);
static int	state_wait_item(struct http_request *);
static int  state_read_item(struct http_request *);
static int	state_error(struct http_request *);
static int	state_done(struct http_request *);

static struct servo_config *CONFIG;

#define REQ_STATE_CONNECT       0
#define REQ_STATE_Q_SESSION     1
#define REQ_STATE_W_SESSION     2
#define REQ_STATE_R_SESSION     3
#define REQ_STATE_Q_ITEM        4
#define REQ_STATE_W_ITEM        5
#define REQ_STATE_R_ITEM        6
#define REQ_STATE_ERROR         7
#define REQ_STATE_DONE          8

struct http_state   servo_session_states[] = {
    { "REQ_STATE_CONNECT",	  state_connect },
    { "REQ_STATE_Q_SESSION",  state_query_session },
    { "REQ_STATE_W_SESSION",  state_wait_session },
    { "REQ_STATE_R_SESSION",  state_read_session },
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
    CONFIG->val_string_size = 1024;
    CONFIG->val_json_size = 1024;
    CONFIG->val_blob_size = 1024000;
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
            return servo_response_error(req, 403, "'Origin' header is not found");
        }
        if (strcmp(origin, CONFIG->allow_origin) != 0) {
            kore_log(LOG_NOTICE, "%s: disallow access - 'Origin' header mismatch %s != %s",
                __FUNCTION__,
                origin, CONFIG->allow_origin);
            return servo_response_error(req, 403, "Origin Access Denied");
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
            return servo_response_error(req, 403, "Client Access Denied");
        }
    }

    /* Initialize request context and run */
    if (req->hdlr_extra == NULL) {
       req->hdlr_extra = servo_create_context(req);
    }
    return (http_state_run(servo_session_states, servo_session_states_size, req));
}

/* Connect to database */
int state_connect(struct http_request *req)
{
    struct servo_context *ctx = req->hdlr_extra;

    if (!kore_pgsql_query_init(&ctx->sql, req, "db", KORE_PGSQL_ASYNC)) {

    	/* If the state was still INIT, we'll try again later. */
    	if (ctx->sql.state == KORE_PGSQL_STATE_INIT) {
    	    req->fsm_state = REQ_STATE_CONNECT;
            kore_log(LOG_DEBUG, "%s: retry state=%d", __FUNCTION__, 
                     req->fsm_state);
    	    return (HTTP_STATE_RETRY);
    	}

        /* Different state means error */
    	kore_pgsql_logerror(&ctx->sql);
    	req->fsm_state = REQ_STATE_ERROR;
    }
    else {
        req->fsm_state = REQ_STATE_Q_SESSION;
    }

    return (HTTP_STATE_CONTINUE);
}

/* Select session for a client */
int state_query_session(struct http_request *req)
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
        
        /* Proceed to REQ_STATE_Q_ITEM with newly created session */
        req->fsm_state = REQ_STATE_Q_ITEM;
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

/* Wait for session query completition
   sends to REQ_STATE_R_SESSION for session 
   state population from DB, orproceed to 
   REQ_STATE_Q_ITEM when query completes.
 */
int state_wait_session(struct http_request *req)
{
    struct servo_context *ctx = req->hdlr_extra;

    kore_log(LOG_DEBUG, "%s: started. context=%p, sql.state=%d", 
        __FUNCTION__, ctx, ctx->sql.state);

    switch (ctx->sql.state) {
    case KORE_PGSQL_STATE_WAIT:
        /* keep waiting */
        return (HTTP_STATE_RETRY);

    case KORE_PGSQL_STATE_COMPLETE:
        /* session query has completed
         * proceed to REQ_STATE_Q_ITEM
         */
        req->fsm_state = REQ_STATE_Q_ITEM;
        break;

    case KORE_PGSQL_STATE_ERROR:
        /* report error and exit state machine */
        req->fsm_state = REQ_STATE_ERROR;
        kore_pgsql_logerror(&ctx->sql);
        break;

    case KORE_PGSQL_STATE_RESULT:
        /* data received, read it */
        req->fsm_state = REQ_STATE_R_SESSION;
        break;

    default:
        /* This MUST be present in order to advance the pgsql state */
        kore_pgsql_continue(req, &ctx->sql);
        break;
    }

    kore_log(LOG_DEBUG, "%s: complete. context=%p, sql.state=%d, next=%d", 
        __FUNCTION__, ctx, ctx->sql.state, req->fsm_state);

    return (HTTP_STATE_CONTINUE);
}

/* Handle session query data fetch */
int state_read_session(struct http_request *req)
{
    int rows = 0;
    char * value;
    struct servo_context *ctx = req->hdlr_extra;

    kore_log(LOG_DEBUG, "%s: started. context=%p, sql.state=%d", 
             __FUNCTION__, ctx, ctx->sql.state);

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
        kore_log(LOG_DEBUG, "%s: selected existing session for client %s - expire_on=%d",
                            __FUNCTION__,
                            ctx->session.client,
                            ctx->session.expire_on);
    }
    else {
        kore_log(LOG_ERR, "selected %d rows, 1 expected", rows);
        return (HTTP_STATE_ERROR);  
    }

    /* Continue processing our query results. */
    kore_pgsql_continue(req, &ctx->sql);

    /* Back to our DB waiting state. */
    req->fsm_state = REQ_STATE_W_SESSION;
    kore_log(LOG_DEBUG, "%s: continue waiting", __FUNCTION__);
    return (HTTP_STATE_CONTINUE);
}

/* Get/Post/Put/Delete item */
int state_query_item(struct http_request *req)
{
    int rc = KORE_RESULT_OK;
    struct servo_context *ctx = req->hdlr_extra;
    char *str_val;
    json_t *json_val;
    void *blob_val;

    switch(req->method) {
        case HTTP_METHOD_POST:
            /* post_item.sql expects 5 arguments: 
             client, key, string, json, blob
            */
            kore_log(LOG_NOTICE, "{%s} POST %s",
                ctx->session.client, req->path);

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
            /* get_item.sql expects 2 arguments:
               client, key
            */
            kore_log(LOG_NOTICE, "{%s} GET %s",
                ctx->session.client, req->path);
            rc = kore_pgsql_query_params(&ctx->sql, 
                                        (const char*)asset_get_item_sql, 
                                        PGSQL_FORMAT_TEXT,
                                        2,
                                        ctx->session.client,
                                        req->path);
        break;
    }

    if (rc != KORE_RESULT_OK) {
        kore_pgsql_logerror(&ctx->sql);
        return HTTP_STATE_ERROR;
    }

    return HTTP_STATE_CONTINUE;
}

int state_wait_item(struct http_request *req)
{
    int rc = KORE_RESULT_OK;
    struct servo_context *ctx = req->hdlr_extra;

    kore_log(LOG_DEBUG, "%s: started. context=%p", 
        __FUNCTION__, ctx);

    return rc;
}

int state_read_item(struct http_request *req)
{
    int rc = KORE_RESULT_OK;
    struct servo_context *ctx = req->hdlr_extra;

    kore_log(LOG_DEBUG, "%s: started. context=%p", 
        __FUNCTION__, ctx);

    return rc;
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