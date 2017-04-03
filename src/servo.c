#include "servo.h"
#include "util.h"
#include "assets.h"

struct servo_config *CONFIG;

struct http_state   servo_session_states[] = {

    { "REQ_STATE_INIT",       servo_state_init  },
    { "REQ_STATE_QUERY",      servo_state_query },
    { "REQ_STATE_WAIT",       servo_state_wait  },
    { "REQ_STATE_READ",       servo_state_read  },

    { "REQ_STATE_ERROR",      state_error },
    { "REQ_STATE_DONE",       state_done  },
};

#define servo_session_states_size (sizeof(servo_session_states) \
    / sizeof(servo_session_states[0]))

const char *
servo_state_text(int s) 
{
    return servo_session_states[s].name;
}

const char *
sql_state_text(int s)
{   
    return SQL_STATE_NAMES[s];
}

const char *
servo_request_state(struct http_request * req)
{
    return servo_state_text(req->fsm_state);
}

void
servo_delete_context(struct http_request *req)
{
    struct servo_context    *ctx;

    ctx = http_state_get(req);
    kore_pgsql_cleanup(&ctx->sql);

    if (ctx->err != NULL)
        kore_free(ctx->err);
    if (ctx->client != NULL)
        kore_free(ctx->client);
    if (ctx->val_str != NULL)
        kore_free(ctx->val_str);
    if (ctx->val_json != NULL)
        json_decref(ctx->val_json);
    if (ctx->val_blob != NULL)
        kore_free(ctx->val_blob);
    if (ctx->token)
        jwt_free(ctx->token);
    
    http_state_cleanup(req);
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
    CONFIG->jwt_key = NULL;
    CONFIG->jwt_key_len = 0;
    CONFIG->jwt_alg = JWT_ALG_NONE;

    if (!servo_read_config(CONFIG)) {
        kore_log(LOG_ERR, "%s: servo is not configured", __FUNCTION__);
        return (KORE_RESULT_ERROR);
    }

    if (CONFIG->jwt_key == NULL && CONFIG->jwt_alg != JWT_ALG_NONE) {
        kore_log(LOG_NOTICE, "no key given for auth, using random");
        CONFIG->jwt_key_len = 16;
        CONFIG->jwt_key = kore_malloc(CONFIG->jwt_key_len);
        CONFIG->jwt_key = servo_random_string(CONFIG->jwt_key,
                                              CONFIG->jwt_key_len);
    }

    kore_log(LOG_NOTICE, "started worker pid: %d", (int)getpid());
    if (CONFIG->jwt_alg != JWT_ALG_NONE) {
        kore_log(LOG_NOTICE, "  auth key: %s", CONFIG->jwt_key);
    }
    else
        kore_log(LOG_NOTICE, "  auth key: disabled");

    kore_log(LOG_NOTICE, "  public mode: %s", CONFIG->public_mode != 0 ? "yes" : "no");
    kore_log(LOG_NOTICE, "  session ttl: %zu seconds", CONFIG->session_ttl);
    kore_log(LOG_NOTICE, "  max sessions: %zu", CONFIG->max_sessions);
    if (CONFIG->allow_origin != NULL)
        kore_log(LOG_NOTICE, "  allow origin: %s", CONFIG->allow_origin);
    if (CONFIG->allow_ipaddr != NULL)
        kore_log(LOG_NOTICE, "  allow ip address: %s", CONFIG->allow_ipaddr);
    
    kore_pgsql_register(DBNAME, CONFIG->database);
    
    return (KORE_RESULT_OK);
}

int
servo_init_context(struct servo_context *ctx)
{
    uuid_t    client_uuid;

    // set empty defaults
    ctx->client = NULL;
    ctx->status = 200;
    ctx->err = NULL;
    ctx->token = NULL;
    ctx->val_sz = 0;
    ctx->val_str = NULL;
    ctx->val_json = NULL;
    ctx->val_blob = NULL;

    /* read and write strings by default */
    ctx->in_content_type = SERVO_CONTENT_STRING;
    ctx->out_content_type = SERVO_CONTENT_STRING;

    /* Generate new client token and init fresh session */
    uuid_generate(client_uuid);

    ctx->client = kore_malloc(CLIENT_UUID_LEN);
    uuid_unparse(client_uuid, ctx->client);
    kore_log(LOG_DEBUG, "new client without identifier, generated {%s}",
             ctx->client);

    if (jwt_new(&ctx->token) != 0) {
        kore_log(LOG_ERR, "%s: failed to allocate jwt",
                 __FUNCTION__);
        ctx->token = NULL;
        return (KORE_RESULT_ERROR);
    }

    if (CONFIG->jwt_alg != JWT_ALG_NONE)
        if (jwt_set_alg(ctx->token, 
                        CONFIG->jwt_alg,
                        (const unsigned char *)CONFIG->jwt_key,
                         CONFIG->jwt_key_len) != 0) {
            kore_log(LOG_ERR, "%s: failed set token alg",
                     __FUNCTION__);
            jwt_free(ctx->token);
            ctx->token = NULL;
            return (KORE_RESULT_ERROR);
        }

    if (jwt_add_grant(ctx->token, "id", ctx->client) != 0) {
        kore_log(LOG_ERR, "%s: failed add grant to jwt",
                 __FUNCTION__);
        jwt_free(ctx->token);
        ctx->token = NULL;
        return (KORE_RESULT_ERROR);
    }

    return (KORE_RESULT_OK);
}

void
servo_write_context_token(struct http_request *req)
{
    struct servo_context    *ctx;
    char                    *token;
    struct kore_buf         *token_hdr;

    ctx = http_state_get(req);
    token = jwt_encode_str(ctx->token);
    token_hdr = kore_buf_alloc(HTTP_HEADER_MAX_LEN);
    kore_buf_append(token_hdr, AUTH_TYPE_PREFIX, strlen(AUTH_TYPE_PREFIX));
    kore_buf_append(token_hdr, token, strlen(token));
    free(token);

    http_response_header(req, AUTH_HEADER,
                         kore_buf_stringify(token_hdr, NULL));
    kore_buf_free(token_hdr);
}

int
servo_read_context_token(struct http_request *req)
{
    int                      n;
    struct servo_context    *ctx;
    char                    *token_hdr,
                            *hdr_parts[3];
    const char              *client_id;

    ctx = http_state_get(req);
    if (ctx->token != NULL || ctx->client != NULL) {
        kore_log(LOG_ERR, "%s: trying to read non-empty context",
                          __FUNCTION__);
        return (KORE_RESULT_ERROR);
    }

    if (!http_request_header(req, AUTH_HEADER, &token_hdr)) {
        kore_log(LOG_DEBUG, "no token header sent");
        return (KORE_RESULT_ERROR);
    }

    n = kore_split_string(token_hdr, " ", hdr_parts, 2);
    if (n != 2) {
        kore_log(LOG_ERR, "%s: invalid header format - '%s'",
                          __FUNCTION__,
                          token_hdr);
        return (KORE_RESULT_ERROR);
    }

    kore_log(LOG_DEBUG, "token of '%s' = '%s'",
                        hdr_parts[0],
                        hdr_parts[1]);
    
    /* parse and verify json web token */
    if (jwt_decode(&ctx->token, 
                    hdr_parts[1],
                    (const unsigned char *)CONFIG->jwt_key,
                    CONFIG->jwt_key_len) != 0) {
        kore_log(LOG_ERR, "%s: invalid json web token received: '%s'",
                 __FUNCTION__,
                 hdr_parts[1]);
        return (KORE_RESULT_ERROR);
    }

    client_id = NULL;
    client_id = jwt_get_grant(ctx->token, "id");
    if (client_id == NULL) {
        kore_log(LOG_ERR, "%s: failed to get client id from token",
                 __FUNCTION__);
        jwt_free(ctx->token);
        ctx->token = NULL;
        ctx->client = NULL;
        return (KORE_RESULT_ERROR);
    }

    ctx->client = kore_strdup(client_id);
    kore_log(LOG_NOTICE, "existing client {%s}", ctx->client);
    return (KORE_RESULT_OK);
}

int 
servo_start(struct http_request *req)
{
    if (!http_state_exists(req)) {
        http_state_create(req, sizeof(struct servo_context));
    }
    return (http_state_run(servo_session_states, servo_session_states_size, req));
}

int
servo_connect_db(struct http_request *req, int retry_step, int success_step, int error_step)
{
    struct servo_context    *ctx = http_state_get(req);
    
    kore_pgsql_cleanup(&ctx->sql);
    kore_pgsql_init(&ctx->sql);
    kore_pgsql_bind_request(&ctx->sql, req);

    if (!kore_pgsql_setup(&ctx->sql, DBNAME, KORE_PGSQL_ASYNC)) {

        /* If the state was still INIT, we'll try again later. */
        if (ctx->sql.state == KORE_PGSQL_STATE_INIT) {
            req->fsm_state = retry_step;
            kore_log(LOG_ERR, "retrying connection, sql state is '%s'",
                sql_state_text(ctx->sql.state));
            return (HTTP_STATE_RETRY);
        }

        /* Different state means error */
        kore_pgsql_logerror(&ctx->sql);
        ctx->status = 500;
        req->fsm_state = error_step;
        kore_log(LOG_ERR, "%s: failed to connect to database, sql state is '%s'",
            __FUNCTION__,
            sql_state_text(ctx->sql.state));
        kore_log(LOG_NOTICE,
            "hint: check database connection string in the configuration file.");
    }
    else {
        req->fsm_state = success_step;
    }

    return (HTTP_STATE_CONTINUE);
}

void 
servo_handle_pg_error(struct http_request *req)
{
    struct servo_context *ctx = http_state_get(req);

    // default failure code
    ctx->status = 500;

    if (strstr(ctx->sql.error, "duplicate key value violates unique constraint") != NULL) {
        ctx->status = 409; // Conflict
    }

    if (ctx->err == NULL) {
        ctx->err = kore_strdup(ctx->sql.error);
    }
}

int
servo_wait(struct http_request *req, int read_step, int complete_step, int error_step)
{
    struct servo_context *ctx = http_state_get(req);

    switch (ctx->sql.state) {
    case KORE_PGSQL_STATE_WAIT:
        /* keep waiting */
        kore_log(LOG_DEBUG, "io wating ~> %s", servo_request_state(req));
        return (HTTP_STATE_RETRY);

    case KORE_PGSQL_STATE_COMPLETE:
        req->fsm_state = complete_step;
        kore_log(LOG_DEBUG, "io complete ~> %s", servo_request_state(req));
        break;

    case KORE_PGSQL_STATE_RESULT:
        req->fsm_state = read_step;
        kore_log(LOG_DEBUG, "io reading ~> %s", servo_request_state(req));
        break;

    case KORE_PGSQL_STATE_ERROR:
        req->fsm_state = error_step;
        kore_log(LOG_ERR, "io failed ~> %s.\n%s",
            servo_request_state(req),
            ctx->sql.error);
        servo_handle_pg_error(req);
        break;

    default:
        kore_pgsql_continue(&ctx->sql);
        break;
    }

    return (HTTP_STATE_CONTINUE);
}


/* An error occurred. */
int state_error(struct http_request *req)
{
    struct servo_context    *ctx = http_state_get(req);
    const char              *msg;

    /* Handle redirect */
    if (servo_is_redirect(ctx)) {
        msg = http_status_text(ctx->status);
        kore_log(LOG_DEBUG, "%d: %s ~> '%s' to {%s}", 
            ctx->status,
            msg,
            req->path,
            ctx->client);

        http_response(req, ctx->status, msg, sizeof(msg));
        servo_delete_context(req);
        return (HTTP_STATE_COMPLETE);
    }

    if (servo_is_success(ctx)) {
        ctx->status = 500;
        kore_log(LOG_DEBUG, "no error status set, default=500");
    }

    kore_log(LOG_ERR, "%d: %s, sql state: %s to {%s}", 
        ctx->status, 
        http_status_text(ctx->status), 
        sql_state_text(ctx->sql.state),
        ctx->client);
    servo_response_status(req, ctx->status, 
        ctx->err != NULL ? ctx->err : http_status_text(ctx->status));

    servo_delete_context(req);
    return (HTTP_STATE_COMPLETE);
}

/* Request was completed successfully. */
int state_done(struct http_request *req)
{
    struct servo_context    *ctx = http_state_get(req);
    const char              *output;

    if (req->method == HTTP_METHOD_POST ||
        req->method == HTTP_METHOD_PUT) 
    {
        switch(ctx->out_content_type) {
            case SERVO_CONTENT_STRING:
                http_response_header(req, "content-type", CONTENT_TYPE_STRING);
                break;
            case SERVO_CONTENT_JSON:
                http_response_header(req, "content-type", CONTENT_TYPE_JSON);
                break;

        }
        /* reply 201 Created on POSTs */
        if (req->method == HTTP_METHOD_POST)
            ctx->status = 201;

        output = http_status_text(ctx->status);
        switch(ctx->out_content_type) {
            default:
            case SERVO_CONTENT_BLOB:
                break;

            case SERVO_CONTENT_STRING:
                http_response(req, ctx->status,
                              output,
                              strlen(output));
                break;
            case SERVO_CONTENT_JSON:
                servo_response_status(req, ctx->status,
                                     output);
                break;
        }

    }
    else if (servo_is_item_request(req)) {

        kore_log(LOG_DEBUG, "serving item size %zu (%s) -> (%s) to {%s}",
            ctx->val_sz,
            SERVO_CONTENT_NAMES[ctx->in_content_type],
            SERVO_CONTENT_NAMES[ctx->out_content_type],
            ctx->client);
        
        switch(ctx->out_content_type) {
            default:
            case SERVO_CONTENT_STRING:
                output = servo_item_to_string(ctx);
                http_response_header(req, "content-type", CONTENT_TYPE_STRING);
                http_response(req, ctx->status, 
                              output == NULL ? "" : output,
                              output == NULL ? 0 : strlen(output));
                break;

            case SERVO_CONTENT_JSON:
                output = servo_item_to_json(ctx);
                http_response_header(req, "content-type", CONTENT_TYPE_JSON);
                http_response(req, ctx->status,
                              output == NULL ? "" : output,
                              output == NULL ? 0 : strlen(output));
                break;

            case SERVO_CONTENT_BLOB:
                servo_response_status(req, 403, http_status_text(403));
                break;

        };
    }
    else {
        ctx->status = 403;
        http_response(req, ctx->status, "", 0);
    }
    
    kore_log(LOG_DEBUG, "%d: %s to {%s}",
        ctx->status,
        http_status_text(ctx->status),
        ctx->client);

    servo_delete_context(req);
    return (HTTP_STATE_COMPLETE);
}
