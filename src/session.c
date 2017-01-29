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


    /* read session properties from database */
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
        kore_pgsql_logerror(&ctx->sql);
        req->fsm_state = REQ_STATE_ERROR;
    }

    kore_log(LOG_DEBUG, "session query complete, continue to %s", 
        servo_request_state(req));
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
        /* no session found, create a new one */
        kore_log(LOG_NOTICE, "no session not found for {%s}",
            ctx->session.client);

        ctx->session.expire_on = time(NULL) + CONFIG->session_ttl;
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
