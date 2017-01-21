#include "servo.h"
#include "util.h"
#include "assets.h"

struct servo_config *CONFIG;

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

static char* DBNAME = "servo-store";

#define STATE_NAME(s) servo_session_states[s].name
#define REQ_STATE(req) STATE_NAME(req->fsm_state)

char    *SQL_STATE_NAMES[] = {
    "<null>",   // NULL
    "init",     // KORE_PGSQL_STATE_INIT
    "wait",     // KORE_PGSQL_STATE_WAIT
    "result",   // KORE_PGSQL_STATE_RESULT
    "error",    // KORE_PGSQL_STATE_ERROR
    "done",     // KORE_PGSQL_STATE_DONE
    "complete"  // KORE_PGSQL_STATE_COMPLETE
};

#define servo_session_states_size (sizeof(servo_session_states) \
    / sizeof(servo_session_states[0]))

struct servo_context *
servo_create_context(struct http_request *req)
{
    struct servo_context *ctx;

    ctx = kore_malloc(sizeof(struct servo_context));
    ctx->status = 200;
    memset(ctx->session.client, 0, sizeof(ctx->session.client));
    memset(&ctx->sql, 0, sizeof(struct kore_pgsql));

    /* read and write strings by default */
    ctx->in_content_type = SERVO_CONTENT_STRING;
    ctx->out_content_type = SERVO_CONTENT_STRING;

    kore_log(LOG_NOTICE, "initialized request context");
    return ctx;
}

int
servo_put_session(struct servo_session *s)
{
    struct kore_pgsql   sql;
    char                sexpire_on[BUFSIZ];

    if (!kore_pgsql_query_init(&sql, NULL, DBNAME, KORE_PGSQL_SYNC)) {
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
                                 s->client, 
                                 strlen(s->client),
                                 PGSQL_FORMAT_TEXT,
                                 sexpire_on,
                                 strlen(s->client),
                                 PGSQL_FORMAT_TEXT)) {
        kore_log(LOG_ERR, "%s: failed to run query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    kore_pgsql_cleanup(&sql);
    kore_log(LOG_NOTICE, "created session %s", s->client);
    return (KORE_RESULT_OK);
}


int
servo_is_success(struct servo_context *ctx)
{
    if (ctx->status >= 200 && ctx->status < 300) {
        return 1;
    }

    return 0;
}

int
servo_is_redirect(struct servo_context *ctx)
{
    if (ctx->status >= 300 && ctx->status < 400)
        return 1;

    return 0;
}

int
servo_init(int state)
{
    CONFIG = kore_malloc(sizeof(struct servo_config));
    memset(CONFIG, 0, sizeof(struct servo_config));

    /* Configuration defaults */
    CONFIG->public_mode = 0;
    CONFIG->session_ttl = 300;
    CONFIG->max_sessions = 10;
    CONFIG->string_size = 255;
    CONFIG->json_size = 1024;
    CONFIG->blob_size = 4096;
    CONFIG->allow_origin = NULL;
    CONFIG->allow_ipaddr = NULL;

    if (!servo_read_config(CONFIG)) {
        kore_log(LOG_ERR, "%s: servo is not configured", __FUNCTION__);
        return (KORE_RESULT_ERROR);
    }

    kore_log(LOG_NOTICE, "started worker pid: %d", (int)getpid());
    kore_log(LOG_NOTICE, "  public mode: %s", CONFIG->public_mode != 0 ? "yes" : "no");
    kore_log(LOG_NOTICE, "  session ttl: %d seconds", CONFIG->session_ttl);
    kore_log(LOG_NOTICE, "  max sessions: %d", CONFIG->max_sessions);
    if (CONFIG->allow_origin != NULL)
        kore_log(LOG_NOTICE, "  allow origin: %s", CONFIG->allow_origin);
    if (CONFIG->allow_ipaddr != NULL)
        kore_log(LOG_NOTICE, "  allow ip address: %s", CONFIG->allow_ipaddr);
    
    kore_pgsql_register(DBNAME, CONFIG->postgresql);
    
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
    struct servo_context    *ctx = req->hdlr_extra;
    
    kore_log(LOG_DEBUG, "connecting... (currerly %s)",
        SQL_STATE_NAMES[ctx->sql.state]);

    kore_pgsql_cleanup(&ctx->sql);
    if (!kore_pgsql_query_init(&ctx->sql, req, DBNAME, KORE_PGSQL_ASYNC)) {

        /* If the state was still INIT, we'll try again later. */
        if (ctx->sql.state == KORE_PGSQL_STATE_INIT) {
            req->fsm_state = retry_step;
            kore_log(LOG_DEBUG, "retrying db connection...");
            return (HTTP_STATE_RETRY);
        }

        /* Different state means error */
        kore_pgsql_logerror(&ctx->sql);
        ctx->status = 500;
        req->fsm_state = error_step;
        kore_log(LOG_ERR, "%s: failed to connect to '%s' >> %s",
            __FUNCTION__,
            DBNAME,
            REQ_STATE(req));
        kore_log(LOG_NOTICE,
            "hint: check database connection string in the configuration file.");
    }
    else {
        req->fsm_state = success_step;
        kore_log(LOG_DEBUG, "connected to db >> %s", REQ_STATE(req));
    }

    return (HTTP_STATE_CONTINUE);
}

int
servo_wait(struct http_request *req, int read_step, int complete_step, int error_step)
{
    struct servo_context *ctx = req->hdlr_extra;

    switch (ctx->sql.state) {
    case KORE_PGSQL_STATE_WAIT:
        /* keep waiting */
        kore_log(LOG_DEBUG, "io wating >> %s", REQ_STATE(req));
        return (HTTP_STATE_RETRY);

    case KORE_PGSQL_STATE_COMPLETE:
        req->fsm_state = complete_step;
        kore_log(LOG_DEBUG, "io complete >> %s", REQ_STATE(req));
        break;

    case KORE_PGSQL_STATE_RESULT:
        req->fsm_state = read_step;
        kore_log(LOG_DEBUG, "io reading >> %s", REQ_STATE(req));
        break;

    case KORE_PGSQL_STATE_ERROR:
        req->fsm_state = error_step;
        ctx->status = 500;
        kore_log(LOG_ERR, "%s: io failed >> %s",
            __FUNCTION__,
            REQ_STATE(req));
        kore_pgsql_logerror(&ctx->sql);
        break;

    default:
        /* This MUST be present in order to advance the pgsql state */
        kore_pgsql_continue(req, &ctx->sql);
        break;
    }

    return (HTTP_STATE_CONTINUE);
}


/* An error occurred. */
int state_error(struct http_request *req)
{
    struct servo_context    *ctx = req->hdlr_extra;
    struct kore_buf         *msgbuf;
    char                    *err;
    const static size_t      codesz = 12;
    char                     code[codesz];

    /* Handle redirect */
    if (servo_is_redirect(ctx)) {
        switch(ctx->status) {
            default:
            case 300: 
                err = "Multiple Choice";
                break;
            case 301:
                err = "Moved Permanently";
                break;
            case 302:
                err = "Found";
                break;
            case 303:
                err = "See Other";
                break;
            case 304:
                err = "Not Modified";
                break;
        };
        kore_log(LOG_DEBUG, "redirecting client '%d: %s' %s", 
            ctx->status, err, req->path);
        http_response(req, ctx->status, err, strlen(err));
        return (HTTP_STATE_COMPLETE);
    }

    /* Report error */
    if (servo_is_success(ctx)) {
        ctx->status = 500;
        kore_log(LOG_DEBUG, "no error status set, default=500");
    }
    snprintf(code, codesz, "%d", ctx->status);

    msgbuf = kore_buf_alloc(1024);
    kore_buf_append(msgbuf, asset_error_html, asset_len_error_html);
    kore_buf_replace_string(msgbuf, "$(status)", code, codesz);
    switch(ctx->status) {
        case 400:
            err = "Bad Request";
            break;
        case 401:
            err = "Unauthorized";
            break;
        case 403:
            err = "Forbidden";
            break;
        case 404:
            err = "Not Found";
            break;
        default:
        case 500:
            err = "Internal Server Error";
            break;
    }
    kore_buf_replace_string(msgbuf, "$(message)", err, strlen(err));

    kore_log(LOG_ERR, "request failed with '%d: %s', sql state: %s", 
        ctx->status, err, 
        SQL_STATE_NAMES[ctx->sql.state]);
    kore_pgsql_cleanup(&ctx->sql);

    servo_response_error(req, ctx->status, err);
    return (HTTP_STATE_COMPLETE);
}

/* Request was completed successfully. */
int state_done(struct http_request *req)
{
    struct servo_context    *ctx;
    char                    *msg, *output;
    const static size_t      codesz = 12;
    char                     code[codesz];

    ctx = (struct servo_context *)req->hdlr_extra;
    snprintf(code, codesz, "%d", ctx->status);

    switch(ctx->status) {
        default:
        case 200:
            msg = "Complete";
            break;

        case 201:
            msg = "Created";
            break;

    };

    
    kore_log(LOG_DEBUG, "request complete with '%d: %s', client {%s}",
        ctx->status, msg, ctx->session.client);

    switch(ctx->out_content_type) {
        default:
        case SERVO_CONTENT_STRING:
            output = servo_item_to_string(ctx);
            http_response_header(req, "content-type", CONTENT_TYPE_STRING);
            http_response(req, ctx->status, output, strlen(output));
            break;

        case SERVO_CONTENT_JSON:
            output = servo_item_to_json(ctx);
            http_response_header(req, "content-type", CONTENT_TYPE_JSON);
            http_response(req, ctx->status, output, strlen(output));
            break;

        case SERVO_CONTENT_BLOB:
            servo_response_error(req, 403, "No Supported");
            break;

    };

    kore_pgsql_cleanup(&ctx->sql);
    return (HTTP_STATE_COMPLETE);
}

char *
servo_item_to_string(struct servo_context *ctx)
{
    char    *b64;

    switch(ctx->in_content_type) {
        case SERVO_CONTENT_STRING:
            return ctx->str_val;
        case SERVO_CONTENT_JSON:
            return json_dumps(ctx->json_val, JSON_INDENT(2));
        case SERVO_CONTENT_BLOB:
            kore_base64_encode(ctx->blob_val, ctx->val_sz, &b64);
            return b64;
    }

    return NULL;
}

char *
servo_item_to_json(struct servo_context *ctx)
{
    /* FIXME: apply json selectors here */
    return servo_item_to_string(ctx);
}