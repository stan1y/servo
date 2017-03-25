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
servo_purge_session(struct servo_session *s)
{
    struct kore_pgsql   sql;

    if (!kore_pgsql_query_init(&sql, NULL, DBNAME, KORE_PGSQL_SYNC)) {
        kore_log(LOG_ERR, "%s: failed to init query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    if (!kore_pgsql_query_params(&sql, 
                                 (const char*)asset_purge_items_sql,
                                 PGSQL_FORMAT_TEXT,
                                 1,
                                 s->client, 
                                 strlen(s->client),
                                 PGSQL_FORMAT_TEXT)) {
        kore_log(LOG_ERR, "%s: failed to run query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    if (!kore_pgsql_query_params(&sql, 
                                 (const char*)asset_purge_session_sql,
                                 PGSQL_FORMAT_TEXT,
                                 1,
                                 s->client, 
                                 strlen(s->client),
                                 PGSQL_FORMAT_TEXT)) {
        kore_log(LOG_ERR, "%s: failed to run query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    kore_pgsql_cleanup(&sql);
    kore_log(LOG_NOTICE, "deleted session for {%s}", s->client);
    return (KORE_RESULT_OK);
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
                                 strlen(sexpire_on),
                                 PGSQL_FORMAT_TEXT)) {
        kore_log(LOG_ERR, "%s: failed to run query", __FUNCTION__);
        kore_pgsql_logerror(&sql);
        return (KORE_RESULT_ERROR);
    }

    kore_pgsql_cleanup(&sql);
    kore_log(LOG_NOTICE, "created new session for {%s}", s->client);
    return (KORE_RESULT_OK);
}

int
state_read_session(struct http_request *req)
{
    int                      rows;
    char                    *sexpire_on;
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
        sexpire_on = kore_pgsql_getvalue(&ctx->sql, 0, 0);
        kore_log(LOG_DEBUG, "found session expire_on=%s", sexpire_on);
        ctx->session.expire_on = atoi(sexpire_on);
        if (ctx->session.expire_on < now) {
            kore_log(LOG_NOTICE, "expired session for {%s}", ctx->session.client);
            kore_log(LOG_DEBUG, "%s < %ld", sexpire_on, now);
            /* recycle session */
            servo_purge_session(&ctx->session);
            ctx->session.expire_on = time(NULL) + CONFIG->session_ttl;
            if (!servo_put_session(&ctx->session)) {
                req->fsm_state = REQ_STATE_ERROR;
                return (HTTP_STATE_CONTINUE);
            }
        }
        else 
            kore_log(LOG_NOTICE, "existing session {%s}, expires on: %s",
                            ctx->session.client,
                            sexpire_on);
    }
    else {
        kore_log(LOG_ERR, "%s: selected %d rows, 1 expected", 
                 __FUNCTION__,
                 rows);
        return (HTTP_STATE_ERROR);  
    }

    /* Continue processing our query results. */
    kore_pgsql_continue(req, &ctx->sql);

    /* Back to our DB waiting state. */
    req->fsm_state = REQ_STATE_W_SESSION;
    return (HTTP_STATE_CONTINUE);
}
