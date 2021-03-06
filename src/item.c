#include "servo.h"
#include "util.h"
#include "assets.h"

int item_sql_update(const char*, struct http_request *, struct kore_buf *, struct http_file *);
int item_sql_query(const char*, struct http_request *);

int
servo_state_init(struct http_request *req)
{
    struct servo_context    *ctx = http_state_get(req);
    char                    *origin = NULL;
    char                     saddr[INET6_ADDRSTRLEN];
    int                      rc;

    /* Filter by Origin header */
    if (CONFIG->allow_origin != NULL) {
        if (!http_request_header(req, "Origin", &origin) && !CONFIG->public_mode) {
            kore_log(LOG_NOTICE, "%s: disallow access - no 'Origin' header sent",
                __FUNCTION__);
            servo_response_status(req, 403, "'Origin' header is not found");
            servo_delete_context(req);
            return (HTTP_STATE_COMPLETE);
        }
        if (strcmp(origin, CONFIG->allow_origin) != 0) {
            kore_log(LOG_NOTICE, "%s: disallow access - 'Origin' header mismatch %s != %s",
                __FUNCTION__,
                origin, CONFIG->allow_origin);
            servo_response_status(req, 403, "Origin Access Denied");
            servo_delete_context(req);
            return (HTTP_STATE_COMPLETE);
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
            servo_response_status(req, 403, "Client Access Denied");
            servo_delete_context(req);
            return (HTTP_STATE_COMPLETE);
        }
    }

    // read header and parse json web token
    if (!servo_read_context_token(req)) {
        if (!servo_init_context(ctx)) {
            // finish request with 500 error 
            servo_response_status(req, 500, http_status_text(500));
            servo_delete_context(req);
            return (HTTP_STATE_COMPLETE);
        }
    }
    kore_log(LOG_DEBUG, "{%s} %s %s started",
                        ctx->client,
                        http_method_text(req->method),
                        req->path);

    // read & init content types
    servo_read_content_types(req);

    // set Access-Control-Allow-Origin header accoring to config
    if (CONFIG->allow_origin != NULL) {
        http_response_header(req, CORS_ALLOWORIGIN_HEADER, CONFIG->allow_origin);
    }
    else {
        http_response_header(req, CORS_ALLOWORIGIN_HEADER, "*");   
    }

    // finish request now for OPTIONS and HEAD methods
    // since we need to respond with CORS *-Allow-* headers
    if (req->method == HTTP_METHOD_OPTIONS ||
        req->method == HTTP_METHOD_HEAD) {
        
        http_response_header(req, CORS_ALLOW_HEADER, AUTH_HEADER);
        http_response_header(req, CORS_ALLOW_HEADER, CONTENT_TYPE_HEADER);
        servo_response_status(req, 200, http_status_text(200));

        kore_log(LOG_ERR, "%s %s by {%s} authorized with %d: %s",
            http_method_text(req->method),
            req->path,
            ctx->client,
            ctx->status,
            http_status_text(ctx->status));

        servo_delete_context(req);
        return (HTTP_STATE_COMPLETE);
    }

    // set Access-Control-Expose-Headers to allow auth header
    // as indicated by Access-Control-Allow-Headers
    http_response_header(req, CORS_EXPOSE_HEADER, AUTH_HEADER);

    // set Authorization header
    servo_write_context_token(req);

    // render stats for client bootstrap
    if (!servo_is_item_request(req)) {
        rc = servo_render_stats(req);
        servo_delete_context(req);
        if (rc != KORE_RESULT_OK) {
            kore_log(LOG_ERR, "%s: failed to render stats.",
                              __FUNCTION__);
            return (HTTP_STATE_ERROR);
        }
        return (HTTP_STATE_COMPLETE);
    }

    // into database io
    return servo_connect_db(req,
                            REQ_STATE_INIT,
                            REQ_STATE_QUERY,
                            REQ_STATE_ERROR);
}

int item_sql_query(const char *asset, struct http_request *req)
{
    /*
        call SQL script [asset] with arguments in order:
        (client, key)
    */
    struct servo_context    *ctx;

    ctx = (struct servo_context*)http_state_get(req);
    return kore_pgsql_query_params(&ctx->sql, 
                                asset, 
                                PGSQL_FORMAT_TEXT,
                                2,
                                // client
                                ctx->client,
                                strlen(ctx->client),
                                PGSQL_FORMAT_TEXT,
                                // key
                                req->path,
                                strlen(req->path),
                                PGSQL_FORMAT_TEXT);
}

int item_sql_update(const char* asset, struct http_request *req, struct kore_buf *body, struct http_file* file)
{
    /*
        call SQL script [asset] with arguments in order:
        (client, key, string, json, blob)
    */
    struct servo_context    *ctx;
    int                      rc;
    char                    *val_str;
    json_error_t             jerr;
    json_t                  *val_json;
    struct kore_buf         *val_bin_buf;
    void                    *val_bin;
    size_t                   val_bin_sz;

    rc = KORE_RESULT_OK;
    ctx = (struct servo_context*)http_state_get(req);
    if (body != NULL) {
        kore_log(LOG_NOTICE, "{%s} reading body %zu bytes (%s) from client",
            ctx->client,
            body->offset,
            SERVO_CONTENT_NAMES[ctx->in_content_type]);
    }
    else if (file != NULL) {
        kore_log(LOG_NOTICE, "{%s} reading file %zu bytes (%s) from client",
            ctx->client,
            file->length,
            SERVO_CONTENT_NAMES[ctx->in_content_type]);   
    }
    else {
        kore_log(LOG_ERR, "{%s} no data from client",
                          ctx->client);
        return (KORE_RESULT_ERROR);
    }

    switch(ctx->in_content_type) {
        default:
        case SERVO_CONTENT_STRING:
            if (body == NULL) {
                kore_log(LOG_ERR, "{%s} no string data in request body",
                          ctx->client);
                return (KORE_RESULT_ERROR);
            }
            val_str = kore_buf_stringify(body, NULL);
            rc = kore_pgsql_query_params(&ctx->sql, 
                                asset, 
                                PGSQL_FORMAT_TEXT,
                                5,
                                // client
                                ctx->client,
                                strlen(ctx->client),
                                PGSQL_FORMAT_TEXT,
                                // key
                                req->path,
                                strlen(req->path),
                                PGSQL_FORMAT_TEXT,
                                // string
                                val_str,
                                strlen(val_str),
                                PGSQL_FORMAT_TEXT, 
                                // json
                                NULL, 0,
                                PGSQL_FORMAT_TEXT,
                                // binary
                                NULL, 0,
                                PGSQL_FORMAT_TEXT);
            break;

        case SERVO_CONTENT_JSON:
            if (body == NULL) {
                kore_log(LOG_ERR, "{%s} no json data in request body",
                          ctx->client);
                return (KORE_RESULT_ERROR);
            }
            val_str = kore_buf_stringify(body, NULL);
            val_json = json_loads(val_str, JSON_ALLOW_NUL, &jerr);
            if (val_json == NULL) {
                ctx->err = kore_malloc(512);
                snprintf(ctx->err, 512,
                         "%s at line: %d, column: %d, pos: %d",
                         jerr.text, jerr.line, jerr.column, jerr.position);
                kore_log(LOG_ERR, "{%s} broken json - %s",
                         ctx->client,
                         ctx->err);
                kore_log(LOG_ERR, "{%s} --start--", ctx->client);
                kore_log(LOG_ERR, "{%s} %s", ctx->client, val_str);
                kore_log(LOG_ERR, "{%s} --end--", ctx->client);
                return (KORE_RESULT_ERROR);
            }
            rc = kore_pgsql_query_params(&ctx->sql, 
                                asset, 
                                PGSQL_FORMAT_TEXT,
                                5,
                                // client
                                ctx->client,
                                strlen(ctx->client),
                                PGSQL_FORMAT_TEXT,
                                // key
                                req->path,
                                strlen(req->path),
                                PGSQL_FORMAT_TEXT,
                                // string
                                NULL, 0,
                                PGSQL_FORMAT_TEXT, 
                                // json
                                val_str, strlen(val_str),
                                PGSQL_FORMAT_TEXT,
                                // binary
                                NULL, 0,
                                PGSQL_FORMAT_TEXT);
            break;

        case SERVO_CONTENT_FORMDATA:
            if (file == NULL) {
                kore_log(LOG_ERR, "{%s} no file data in multipart request",
                          ctx->client);
                return (KORE_RESULT_ERROR);
            }
            val_bin_buf = servo_read_file(file);
            if (val_bin_buf == NULL) {
                kore_log(LOG_ERR, "{%s} failed to read file contents",
                          ctx->client);
                return (KORE_RESULT_ERROR);
            }
            val_bin = kore_buf_stringify(val_bin_buf, &val_bin_sz);
            rc = kore_pgsql_query_params(&ctx->sql, 
                                asset, 
                                PGSQL_FORMAT_TEXT,
                                5,
                                // client
                                ctx->client,
                                strlen(ctx->client),
                                PGSQL_FORMAT_TEXT,
                                // key
                                req->path,
                                strlen(req->path),
                                PGSQL_FORMAT_TEXT,
                                // string
                                NULL, 0,
                                PGSQL_FORMAT_TEXT, 
                                // json
                                NULL, 0,
                                PGSQL_FORMAT_TEXT,
                                // binary
                                val_bin,
                                val_bin_sz,
                                PGSQL_FORMAT_TEXT);
            kore_buf_free(val_bin_buf);
            break;
    }
    return rc;
}

int state_handle_get(struct http_request *req)
{
    /* get_item.sql
     * $1 - client
     * $2 - item key 
     */
    return item_sql_query((const char*)asset_get_item_sql, req);
}

int state_handle_delete(struct http_request *req)
{
    /* delete_item.sql
     * $1 - client
     * $2 - item key 
     */
    return item_sql_query((const char*)asset_delete_item_sql, req);
}

int state_handle_put(struct http_request *req, struct kore_buf *body, struct http_file *file)
{
    /* put_item.sql expects 5 arguments: 
     client, key, string, json, blob
    */
    return item_sql_update((const char*)asset_put_item_sql, req, body, file);
}

int state_handle_post(struct http_request *req, struct kore_buf *body, struct http_file *file)
{
    /* post_item.sql expects 5 arguments: 
     client, key, string, json, blob
    */
    return item_sql_update((const char*)asset_post_item_sql, req, body, file);
}

int
servo_state_query(struct http_request *req)
{
    int                      rc, too_big;
    struct servo_context    *ctx = NULL;
    struct kore_buf         *body = NULL;
    struct http_file        *file = NULL;
    size_t limit                  = 0;

    rc = KORE_RESULT_OK;
    ctx = (struct servo_context*)http_state_get(req);

    /* Check size limitations for body & multipart */
    too_big = 0;
    if (req->method == HTTP_METHOD_POST ||
        req->method == HTTP_METHOD_PUT) {
        
        switch(ctx->in_content_type) {
            /* Read request body */
            default:
            case SERVO_CONTENT_STRING:
            case SERVO_CONTENT_JSON:
                body = servo_read_body(req);
                if (body == NULL) {
                    kore_log(LOG_ERR, "{%s} no request body to handle",
                                      ctx->client);
                    ctx->status = 400;
                    ctx->err = kore_strdup("No request body to handle");
                    req->fsm_state = REQ_STATE_ERROR;
                    return (HTTP_STATE_CONTINUE);
                }
                if (ctx->in_content_type == SERVO_CONTENT_STRING)
                    limit = CONFIG->string_size;
                if (ctx->in_content_type == SERVO_CONTENT_JSON)
                    limit = CONFIG->json_size;

                if (body->offset > limit) {
                    kore_log(LOG_ERR, "{%s} body size is too large. %lu > %lu",
                                      ctx->client,
                                      body->offset,
                                      limit);
                    too_big = 1;
                }
                break;

            /* Read multipart form data */
            case SERVO_CONTENT_FORMDATA:
                http_populate_multipart_form(req);
                file = http_file_lookup(req, "file");
                if (file == NULL) {
                    kore_log(LOG_ERR, "{%s} no 'file' parameter sent in the form data.",
                                      ctx->client);
                    ctx->status = 400;
                    ctx->err = kore_strdup("No 'file' parameter given in multipart/form-data content.");
                    req->fsm_state = REQ_STATE_ERROR;
                    return (HTTP_STATE_CONTINUE);
                }
                if (file->length > CONFIG->blob_size) {
                    kore_log(LOG_ERR, "{%s} file size is too large. %lu > %lu",
                                      ctx->client,
                                      file->length,
                                      CONFIG->blob_size);
                    too_big = 1;
                }
                break;
        };
    }

    /* Size limitations */
    if (too_big) {
        kore_log(LOG_ERR, "{%s} request forbidden.",
                          ctx->client);
        if (body != NULL) kore_buf_free(body);
        ctx->status = 403;
        ctx->err = kore_strdup("Request is too large");
        req->fsm_state = REQ_STATE_ERROR;
        return (HTTP_STATE_CONTINUE);
    }

    /* Handle item operation in http method */
    switch(req->method) {
        case HTTP_METHOD_POST:
            rc = state_handle_post(req, body, file);
            if (body != NULL) kore_buf_free(body);
            break;

        case HTTP_METHOD_PUT:
            rc = state_handle_put(req, body, file);
            if (body != NULL) kore_buf_free(body);
            break;

        case HTTP_METHOD_DELETE:
            rc = state_handle_delete(req);
            break;

        default:
        case HTTP_METHOD_GET:
            rc = state_handle_get(req);
            break;
    }

    if (rc != KORE_RESULT_OK) {
        /* go to error handler
           PGSQL errors are internal 500
           the other case is broken json 
         */
        req->fsm_state = REQ_STATE_ERROR;
        if (ctx->sql.state == KORE_PGSQL_STATE_ERROR) {
            kore_pgsql_logerror(&ctx->sql);
            ctx->status = 500;
        }
        else
            ctx->status = 400;
        return (HTTP_STATE_CONTINUE);
    }

    kore_log(LOG_DEBUG, "{%s} requested io, state: %s, sql: %s, next: %s",
                        ctx->client,
                        servo_state_text(req->fsm_state),
                        sql_state_text(ctx->sql.state),
                        servo_state_text(REQ_STATE_WAIT));
    /* Wait for IO request completition */    
    req->fsm_state = REQ_STATE_WAIT;
    return (HTTP_STATE_CONTINUE);
}

int servo_state_wait(struct http_request *req)
{
    return servo_wait(req, REQ_STATE_READ,
                           REQ_STATE_DONE,
                           REQ_STATE_ERROR);
}

int servo_state_read(struct http_request *req)
{
    int                      rows;               
    struct servo_context    *ctx;
    char                    *val;
    json_error_t            jerr;

    ctx = (struct servo_context*)http_state_get(req);

    if (req->method != HTTP_METHOD_GET) {
        kore_log(LOG_ERR, "{%s} %s %s is forbidden", 
                 ctx->client,
                 http_method_text(req->method),
                 req->path);
        return HTTP_STATE_ERROR;
    }

    rows = 0;
    val = NULL;

    ctx->val_str = NULL;
    ctx->val_json = NULL;
    ctx->val_bin = NULL;
    ctx->val_sz = 0;

    rows = kore_pgsql_ntuples(&ctx->sql);
    if (rows == 0) {
        /* item was not found, report 404 */
        kore_log(LOG_DEBUG, "{%s} nothing selected for key '%s'",
                            ctx->client,
                            req->path);
        ctx->status = 404;
        req->fsm_state = REQ_STATE_ERROR;
        return HTTP_STATE_CONTINUE;
    }
    else if (rows == 1) {
        /* found existing session record */
        val = kore_pgsql_getvalue(&ctx->sql, 0, 0);
        if (val != NULL && strlen(val) > 0) {
            ctx->val_str = kore_strdup(val);
            ctx->val_sz = strlen(val);
        }

        val = kore_pgsql_getvalue(&ctx->sql, 0, 1);
        if (val != NULL && strlen(val) > 0) {
            ctx->val_json = json_loads(val, JSON_ALLOW_NUL, &jerr);
            if (ctx->val_json == NULL) {
                kore_log(LOG_ERR, "{%s} malformed json read from database for key '%s'",
                                  ctx->client,
                                  req->path);
                return (HTTP_STATE_ERROR);
            }
            ctx->val_sz = strlen(val);
        }
        val = kore_pgsql_getvalue(&ctx->sql, 0, 2);
        if (val != NULL && strlen(val) > 0) {
            ctx->val_bin = kore_strdup(val);
            ctx->val_sz = strlen(ctx->val_bin);
        }

        /* since we've read item, update in_content_type
           because it indicated type we store
         */
        if (ctx->val_str != NULL)
            ctx->in_content_type = SERVO_CONTENT_STRING;
        if (ctx->val_json != NULL)
            ctx->in_content_type = SERVO_CONTENT_JSON;
        if (ctx->val_bin != NULL)
            ctx->in_content_type = SERVO_CONTENT_FORMDATA;
    }
    else {
        kore_log(LOG_ERR, "{%s} selected %d rows for key '%s', but 1 expected",
            ctx->client,
            rows,
            req->path);
        return (HTTP_STATE_ERROR);  
    }

    /* Continue processing our query results. */
    kore_pgsql_continue(&ctx->sql);

    /* Back to our DB waiting state. */
    req->fsm_state = REQ_STATE_WAIT;
    return (HTTP_STATE_CONTINUE);
}
