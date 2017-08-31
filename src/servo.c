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

char* SQL_STATE_NAMES[] = {
    "<null>",   // NULL
    "init",     // KORE_PGSQL_STATE_INIT
    "wait",     // KORE_PGSQL_STATE_WAIT
    "result",   // KORE_PGSQL_STATE_RESULT
    "error",    // KORE_PGSQL_STATE_ERROR
    "done",     // KORE_PGSQL_STATE_DONE
    "complete"  // KORE_PGSQL_STATE_COMPLETE
};

char* SERVO_CONTENT_NAMES[] = {
    CONTENT_TYPE_STRING,
    CONTENT_TYPE_JSON,
    CONTENT_TYPE_FORMDATA,
    CONTENT_TYPE_BASE64,
    CONTENT_TYPE_HTML
};

char* DBNAME = "servo-store";

/* Utilities */

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

    kore_log(LOG_NOTICE, "{%s} << close session, state: %s, sql: %s",
                         ctx->client,
                         servo_request_state(req),
                         sql_state_text(ctx->sql.state));
    if (ctx->err != NULL)
        kore_free(ctx->err);
    if (ctx->client != NULL)
        kore_free(ctx->client);
    if (ctx->val_str != NULL)
        kore_free(ctx->val_str);
    if (ctx->val_json != NULL)
        json_decref(ctx->val_json);
    if (ctx->val_bin != NULL)
        kore_free(ctx->val_bin);
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

    if (servo_test_db() != KORE_RESULT_OK ) {
      kore_log(LOG_NOTICE, "initializing database...");
      if (servo_recreate_db() != KORE_RESULT_OK) {
        kore_log(LOG_ERR, "failed to setup database");
        return (KORE_RESULT_ERROR);
      }
    }

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
    ctx->val_bin = NULL;

    /* read and write strings by default */
    ctx->in_content_type = SERVO_CONTENT_STRING;
    ctx->out_content_type = SERVO_CONTENT_STRING;

    /* Generate new client token and init fresh session */
    uuid_generate(client_uuid);

    ctx->client = kore_malloc(CLIENT_UUID_LEN);
    uuid_unparse(client_uuid, ctx->client);

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

    kore_log(LOG_NOTICE, "{%s} >> initialized session", ctx->client);
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
    char                    *t, *token_hdr,
                            *hdr_parts[3];
    const char              *client_id;
    jwt_t                   *token;

    if (!http_request_header(req, AUTH_HEADER, &t)) {
        return (KORE_RESULT_ERROR);
    }

    token_hdr = kore_strdup(t);
    n = kore_split_string(token_hdr, " ", hdr_parts, 3);
    kore_free(token_hdr);
    if (n != 2) {
        kore_log(LOG_ERR, "%s: invalid header format, n=%d - '%s'",
                          __FUNCTION__,
                          n, t);
        return (KORE_RESULT_ERROR);
    }
    /* parse and verify json web token */
    if (jwt_decode(&token,
                   hdr_parts[1],
                   (const unsigned char *)CONFIG->jwt_key,
                   CONFIG->jwt_key_len) != 0) {
        kore_log(LOG_ERR, "%s: invalid json web token received: '%s'",
                 __FUNCTION__,
                 hdr_parts[1]);
        return (KORE_RESULT_ERROR);
    }

    client_id = jwt_get_grant(token, "id");
    if (client_id == NULL) {
        kore_log(LOG_ERR, "%s: failed to get client id from token",
                 __FUNCTION__);
        return (KORE_RESULT_ERROR);
    }

    /* get and set http state from token */
    ctx = (struct servo_context *)http_state_get(req);
    if (ctx != NULL && (ctx->token != NULL || ctx->client != NULL)) {
        kore_log(LOG_ERR, "{%s}: trying reset context with {%s}",
                          ctx->client,
                          client_id);
        return (KORE_RESULT_ERROR);
    }
    ctx->token = token;
    ctx->client = kore_strdup(client_id);

    kore_log(LOG_NOTICE, "{%s} >> existing session", ctx->client);
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
servo_render_stats(struct http_request *req)
{
    int                      rc;
    json_t                  *stats;
    struct servo_context    *ctx;
    time_t                   last_read, last_write;

    rc = KORE_RESULT_OK;
    ctx = (struct servo_context *)http_state_get(req);
    // FIXME: real stats here
    last_read = time(NULL);
    last_write = time(NULL);
    stats = json_pack("{s:s s:s s:s s:i}",
              "client",      ctx->client,
              "last_read",   servo_format_date(&last_read),
              "last_write",  servo_format_date(&last_write),
              "session_ttl", CONFIG->session_ttl);
    servo_response_json(req, 200, stats);
    json_decref(stats);

    kore_log(LOG_NOTICE, "{%s} render stats", ctx->client);
    return rc;
}

int
servo_connect_db(struct http_request *req, int retry_step, int success_step, int error_step)
{
    struct servo_context    *ctx = http_state_get(req);

    kore_pgsql_cleanup(&ctx->sql);
    kore_pgsql_init(&ctx->sql);
    kore_pgsql_bind_request(&ctx->sql, req);

    kore_log(LOG_DEBUG, "{%s} connecting, sql: %s",
                        ctx->client,
                        sql_state_text(ctx->sql.state));

    if (!kore_pgsql_setup(&ctx->sql, DBNAME, KORE_PGSQL_ASYNC)) {
        kore_pgsql_logerror(&ctx->sql);

        /* If the state was still INIT, we'll try again later. */
        if (ctx->sql.state == KORE_PGSQL_STATE_INIT) {
            req->fsm_state = retry_step;
            kore_log(LOG_ERR, "{%s} retrying connection, sql: %s",
                              ctx->client,
                              sql_state_text(ctx->sql.state));
            return (HTTP_STATE_RETRY);
        }

        /* Different state means error */
        kore_pgsql_logerror(&ctx->sql);
        ctx->status = 500;
        req->fsm_state = error_step;
        kore_log(LOG_ERR, "{%s} failed to connect to database, sql: %s",
            ctx->client,
            sql_state_text(ctx->sql.state));
        kore_log(LOG_NOTICE,
            "hint: check database connection string in the configuration file.");
    }
    else {
        kore_log(LOG_DEBUG, "{%s} connected, state: %s, sql: %s, next: %s",
                            ctx->client,
                            servo_state_text(req->fsm_state),
                            sql_state_text(ctx->sql.state),
                            servo_state_text(success_step));
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
        kore_log(LOG_DEBUG, "{%s} io wating, state: %s, sql: %s",
                            ctx->client,
                            servo_request_state(req),
                            sql_state_text(ctx->sql.state));
        return (HTTP_STATE_RETRY);

    case KORE_PGSQL_STATE_COMPLETE:
        req->fsm_state = complete_step;
        kore_log(LOG_DEBUG, "{%s} io complete, state: %s, sql: %s",
                            ctx->client,
                            servo_request_state(req),
                            sql_state_text(ctx->sql.state));
        break;

    case KORE_PGSQL_STATE_RESULT:
        req->fsm_state = read_step;
        kore_log(LOG_DEBUG, "{%s} io reading, state: %s, sql: %s",
                            ctx->client,
                            servo_request_state(req),
                            sql_state_text(ctx->sql.state));
        break;

    case KORE_PGSQL_STATE_ERROR:
        req->fsm_state = error_step;
        kore_log(LOG_ERR, "{%s} io failed, state: %s, sql: %s, sql error: %s",
            ctx->client,
            servo_request_state(req),
            sql_state_text(ctx->sql.state),
            ctx->sql.error);
        servo_handle_pg_error(req);
        break;

    default:
        // kore_log(LOG_DEBUG, "{%s} waiting for io... state: %s, sql: %s",
        //                     ctx->client,
        //                     servo_request_state(req),
        //                     sql_state_text(ctx->sql.state));
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
        http_response(req, ctx->status, msg, sizeof(msg));
        kore_log(LOG_DEBUG, "{%s} %s %s redirected with %d: %s",
            ctx->client,
            http_method_text(req->method),
            req->path,
            ctx->status,
            msg);
        servo_delete_context(req);
        return (HTTP_STATE_COMPLETE);
    }

    if (servo_is_success(ctx)) {
        ctx->status = 500;
        kore_log(LOG_DEBUG, "{%s} no error status set, default is 500",
                            ctx->client);
    }

    servo_response_status(req, ctx->status,
        ctx->err != NULL ? ctx->err : http_status_text(ctx->status));

    kore_log(LOG_ERR, "{%s} sql state on error is %s",
        ctx->client,
        sql_state_text(ctx->sql.state));

    kore_log(LOG_ERR, "{%s} %s %s failed with %d: %s",
        ctx->client,
        http_method_text(req->method),
        req->path,
        ctx->status,
        http_status_text(ctx->status));

    servo_delete_context(req);
    return (HTTP_STATE_COMPLETE);
}

/* Request was completed successfully. */
int state_done(struct http_request *req)
{
    struct servo_context    *ctx = http_state_get(req);
    const char              *output;

    ctx->status = 200;
    if (req->method == HTTP_METHOD_POST ||
        req->method == HTTP_METHOD_PUT)
    {
        /* reply 201 Created on POSTs */
        if (req->method == HTTP_METHOD_POST)
            ctx->status = 201;

        output = http_status_text(ctx->status);
        switch(ctx->out_content_type) {
            default:
            case SERVO_CONTENT_STRING:
                http_response_header(req, CONTENT_TYPE_HEADER, CONTENT_TYPE_STRING);
                http_response(req, ctx->status,
                              output,
                              strlen(output));
                break;

            case SERVO_CONTENT_JSON:
            case SERVO_CONTENT_FORMDATA:
                servo_response_status(req, ctx->status,
                                      output);
                break;
        }
        kore_log(LOG_DEBUG, "{%s} saved item %zu bytes, type: %s",
                 ctx->client,
                 ctx->val_sz,
                 SERVO_CONTENT_NAMES[ctx->in_content_type]);
    }
    else if (servo_is_item_request(req)) {

        switch(ctx->out_content_type) {
            default:
            case SERVO_CONTENT_STRING:
                output = servo_item_to_string(ctx);
                http_response_header(req, CONTENT_TYPE_HEADER, CONTENT_TYPE_STRING);
                http_response(req, ctx->status,
                              output == NULL ? "" : output,
                              output == NULL ? 0 : strlen(output));
                break;

            case SERVO_CONTENT_JSON:
                output = servo_item_to_json(ctx);
                http_response_header(req, CONTENT_TYPE_HEADER, CONTENT_TYPE_JSON);
                http_response(req, ctx->status,
                              output == NULL ? "" : output,
                              output == NULL ? 0 : strlen(output));
                break;

            case SERVO_CONTENT_FORMDATA:
                http_response_header(req, CONTENT_TYPE_HEADER, CONTENT_TYPE_FORMDATA);
                servo_response_status(req, 403, http_status_text(403));
                break;

        };

        kore_log(LOG_DEBUG, "{%s} wrote item %zu bytes, src type: %s, dst type: %s",
                 ctx->client,
                 ctx->val_sz,
                 SERVO_CONTENT_NAMES[ctx->in_content_type],
                 SERVO_CONTENT_NAMES[ctx->out_content_type]);
    }
    else {
        ctx->status = 403;
        http_response(req, ctx->status, "", 0);
    }

    kore_log(LOG_DEBUG, "{%s} %s %s completed with %d: %s",
        ctx->client,
        http_method_text(req->method),
        req->path,
        ctx->status,
        http_status_text(ctx->status));

    servo_delete_context(req);
    return (HTTP_STATE_COMPLETE);
}
