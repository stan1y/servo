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
    memset(ctx->item, 0, sizeof(ctx->item));

    /* read and write strings by default */
    ctx->in_content_type = SERVO_CONTENT_STRING;
    ctx->out_content_type = SERVO_CONTENT_STRING;
    memset(ctx->item, 0, sizeof(ctx->item));
    strncpy(ctx->item, req->path, sizeof(req->path) - 1);

    kore_log(LOG_DEBUG, "%s: new context for item '%s'",
             __FUNCTION__, ctx->item);

    return ctx;
}

/**
 * Write session information to db (sync)
 */
int servo_put_session(struct servo_session *s)
{
    int rows = 0;
    struct kore_pgsql sql;

    kore_log(LOG_DEBUG, "%s: saving session=%p for client '%s'",
                        __FUNCTION__, s, s->client);

    return KORE_RESULT_OK;

    if (!kore_pgsql_query_init(&sql, NULL, "db", KORE_PGSQL_SYNC)) {
        kore_log(LOG_ERR, "%s: failed to init query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    kore_log(LOG_DEBUG, "%s: sync sql state=%d",
                        __FUNCTION__, sql.state);

    if (!kore_pgsql_query_params(&sql, 
                                 (const char*)"insert into session values ($1, $2)", 
                                 PGSQL_FORMAT_TEXT,
                                 2, 
                                 "aaa", "bbb")) {
        kore_log(LOG_ERR, "%s: failed to run query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    rows = kore_pgsql_ntuples(&sql);
    if (rows != 1) {

        /* We expected 1 row to be affected here */
        kore_log(LOG_ERR, "Expected 1 row to change on insert");
        return (KORE_RESULT_ERROR);
    }

    return (KORE_RESULT_OK);
}

void servo_init_session(struct servo_session *s)
{
    s->expire_on = time(NULL) + CONFIG->session_ttl;
    kore_log(LOG_DEBUG, "%s: initialized session=%p with expire_on=%d",
                        __FUNCTION__,
                        s,
                        s->expire_on);
}


/**
 * Bootstap servo
 */
int servo_init(int state)
{
    CONFIG = kore_malloc(sizeof(struct servo_config));
    memset(CONFIG, 0, sizeof(struct servo_config));

    if (!servo_read_config(CONFIG)) {
        kore_log(LOG_ERR, "%s: failed to read config", __FUNCTION__);
        return (KORE_RESULT_ERROR);
    }

    kore_log(LOG_NOTICE, "----- initializing pid: %d -----", (int)getpid());
    kore_log(LOG_NOTICE, "public mode: %s", CONFIG->public_mode != 0 ? "yes" : "no");
    kore_log(LOG_NOTICE, "session ttl: %d seconds", CONFIG->session_ttl);
    kore_log(LOG_NOTICE, "max sessions: %d", CONFIG->max_sessions);
    kore_log(LOG_NOTICE, "using database at \"%s\"", CONFIG->connect);
    
    kore_pgsql_register("db", CONFIG->connect);
    
    return (KORE_RESULT_OK);
}


/**
 * Servo Session API entry point
 */
int servo_start(struct http_request *req)
{
    char *origin;
    kore_log(LOG_DEBUG, "%s: started (%s)", 
             __FUNCTION__, req->path);

    // filter by Origin header
    if (strlen(req->path) > 0 && !http_request_header(req, "Origin", &origin) && !CONFIG->public_mode) {
        kore_log(LOG_ERR, "%s: no Origin header sent", __FUNCTION__);
        return servo_response_error(req, 403, "'Origin' header is not found");
    }

    // setup context
    if (req->hdlr_extra == NULL) {
       req->hdlr_extra = servo_create_context(req);
       kore_log(LOG_DEBUG, "%s: set context=%p",
             __FUNCTION__, req->hdlr_extra);
    }

    // run state machine
    kore_log(LOG_DEBUG, "%s: step into states", __FUNCTION__);
    return (http_state_run(servo_session_states, servo_session_states_size, req));
}

/* Connect to database */
int state_connect(struct http_request *req)
{
    struct servo_context *ctx = req->hdlr_extra;

    kore_log(LOG_DEBUG, "%s: started. context=%p, sql.state=%d", 
             __FUNCTION__, ctx, ctx->sql.state);

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

    kore_log(LOG_DEBUG, "%s: continue to state=%d", __FUNCTION__, req->fsm_state);
    return (HTTP_STATE_CONTINUE);
}

/* Select session for a client */
int state_query_session(struct http_request *req)
{
    int rc = KORE_RESULT_OK;
    struct servo_context *ctx = req->hdlr_extra;
    char *x_client = NULL;

    kore_log(LOG_DEBUG, "%s: started. context=%p, sql.state=%d", 
             __FUNCTION__, ctx, ctx->sql.state);

    if (!ctx->session.client || strlen(ctx->session.client) == 0) {

        /* Check request header first */
        if (http_request_header(req, "X-Servo-Client", &x_client)) {
            strncpy(ctx->session.client, x_client, sizeof(ctx->session.client) - 1);
            kore_log(LOG_DEBUG, "using header 'X-Servo-Client': %s", x_client);
        }
    }

    if (!ctx->session.client || strlen(ctx->session.client) == 0) {
        
        /* Read cookie next */
        servo_read_cookie(req, "Servo-Client", ctx->session.client);
    }

    if (!ctx->session.client || strlen(ctx->session.client) == 0) {
        
        /* Create new client & session */
        strncpy(ctx->session.client, "new-client-id", sizeof(ctx->session.client) - 1);
        servo_init_session(&ctx->session);
        servo_put_session(&ctx->session);
        kore_log(LOG_NOTICE, "%s: created session for new client '%s'",
                             __FUNCTION__, ctx->session.client);
        

        /* handle item request with newly created session */
        req->fsm_state = REQ_STATE_Q_ITEM;
    }
    else {

        /* Known client
         * lookup existing session with 
         * 'asset_query_session_sql' - expects one arguments
         */

        // run the query
        req->fsm_state = REQ_STATE_W_SESSION;
        rc = kore_pgsql_query_params(&ctx->sql, 
                                    (const char*)asset_query_session_sql, 
                                    PGSQL_FORMAT_TEXT,
                                    1,
                                    ctx->session.client);
        if (rc != KORE_RESULT_OK) {
            kore_log(LOG_ERR, "Failed to execute query.");
            req->fsm_state = REQ_STATE_ERROR;
        }
    }

    // resume state machine when data arrive
    // or go directly to error
    return rc == KORE_RESULT_OK ? HTTP_STATE_CONTINUE : HTTP_STATE_ERROR; 
}

/* For for session data */
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

    return (HTTP_STATE_CONTINUE);
}

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
        servo_init_session(&ctx->session);
        servo_put_session(&ctx->session);
        kore_log(LOG_DEBUG, "%s: recreated session for client '%s'",
                            __FUNCTION__,
                            ctx->session.client);
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
        return HTTP_STATE_ERROR;  
    }

    /* Continue processing our query results. */
    kore_pgsql_continue(req, &ctx->sql);

    /* Back to our DB waiting state. */
    req->fsm_state = REQ_STATE_W_SESSION;
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

    kore_log(LOG_DEBUG, "%s: started. context=%p, sql.state=%d", 
             __FUNCTION__, ctx, ctx->sql.state);

    switch(req->method) {
        case HTTP_METHOD_POST:
            /* post_item.sql expects 5 arguments: 
             client, key, string, json, blob
            */
            kore_log(LOG_DEBUG, "%s: POST item '%s' for client '%s'",
                                __FUNCTION__, ctx->item, ctx->session.client);

            switch(ctx->in_content_type) {
            
                default:
                case SERVO_CONTENT_STRING:
                str_val = servo_request_str_data(req);
                rc = kore_pgsql_query_params(&ctx->sql, 
                                    (const char*)asset_post_item_sql, 
                                    PGSQL_FORMAT_TEXT,
                                    5,
                                    ctx->session.client,
                                    ctx->item,
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
                                    ctx->item,
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
                                    ctx->item,
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
            kore_log(LOG_DEBUG, "%s: GET item '%s' for client '%s'",
                                __FUNCTION__, ctx->item, ctx->session.client);
            rc = kore_pgsql_query_params(&ctx->sql, 
                                        (const char*)asset_get_item_sql, 
                                        PGSQL_FORMAT_TEXT,
                                        2,
                                        ctx->session.client,
                                        ctx->item);
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
    struct servo_context *ctx = req->hdlr_extra;

    kore_log(LOG_DEBUG, "%s: started. context=%p", 
        __FUNCTION__, ctx);

    kore_pgsql_cleanup(&ctx->sql);
    http_response(req, 500, NULL, 0);

    return (HTTP_STATE_COMPLETE);
}

/* Request was completed successfully. */
int state_done(struct http_request *req)
{
    struct servo_context *ctx = req->hdlr_extra;

    kore_log(LOG_DEBUG, "%s: started. context=%p", 
        __FUNCTION__, ctx);

    kore_pgsql_cleanup(&ctx->sql);
    http_response(req, 200, NULL, 0);

    return (HTTP_STATE_COMPLETE);
}