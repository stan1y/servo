#include "servo.h"
#include "util.h"
#include "assets.h"

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
            kore_log(LOG_ERR, "%s: failed to execute query.", __FUNCTION__);
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
state_wait_session(struct http_request *req)
{
    return servo_wait(req, REQ_STATE_R_SESSION,
                           REQ_STATE_C_ITEM,
                           REQ_STATE_ERROR);
}

int
state_read_session(struct http_request *req)
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
