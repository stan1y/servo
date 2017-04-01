#include "servo.h"
#include "assets.h"
#include "util.h"

int servo_render_stats(struct http_request *req)
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
		      "client", ctx->client,
		      "last_read", servo_format_date(&last_read),
              "last_write", servo_format_date(&last_write),
              "session_ttl", CONFIG->session_ttl);
    servo_response_json(req, 200, stats);
    json_decref(stats);
    
    kore_log(LOG_NOTICE, "rendering stats for {%s}", ctx->client);
    return rc;
}

int servo_render_console_js(struct http_request *req)
{
    http_response_header(req, "content-type", "application/javascript");
    return asset_serve_console_js(req);
}

int servo_render_console(struct http_request *req)
{
    int                      rc;
    struct kore_buf         *buf;
    struct servo_context    *ctx;

    rc = KORE_RESULT_OK;
    ctx = (struct servo_context *)http_state_get(req);

    buf = kore_buf_alloc(asset_len_console_html);
    kore_buf_append(buf, asset_console_html, asset_len_console_html);
    kore_buf_replace_string(buf, "$CLIENTID$",
        ctx->client, strlen(ctx->client));

    http_response_header(req, "content-type", "text/html");
    http_response(req, 200, buf->data, buf->offset);
    kore_buf_free(buf);

    kore_log(LOG_NOTICE, "rendering console for {%s}", ctx->client);
    return rc;
}