#include "servo.h"
#include "assets.h"

int servo_render_stats(struct http_request *req)
{
    int                      rc;
    char                     expire_on[80];
    json_t                  *stats;
    struct tm               *expiration;
    struct servo_context    *ctx;

    rc = KORE_RESULT_OK;
    ctx = (struct servo_context*)req->hdlr_extra;
    expiration = gmtime(&ctx->session.expire_on);
    strftime(expire_on, sizeof(expire_on), "%a %Y-%m-%d %H:%M:%S %Z", expiration);
    
    stats = json_pack("{s:s s:s}",
		      "client", ctx->session.client,
		      "expire_on", expire_on);
    servo_response_json(req, 200, stats);
    json_decref(stats);
    
    return rc;
}

int servo_render_console(struct http_request *req)
{
    int                      rc;
    struct kore_buf         *buf;
    struct servo_context    *ctx;

    rc = KORE_RESULT_OK;
    ctx = (struct servo_context *)req->hdlr_extra;
    buf = kore_buf_alloc(asset_len_console_html);
    kore_buf_append(buf, asset_console_html, asset_len_console_html);
    kore_buf_replace_string(buf, "$CLIENTID$",
        ctx->session.client, strlen(ctx->session.client));

    http_response_header(req, "content-type", "text/html");
    http_response(req, 200, buf->data, buf->offset);
    kore_buf_free(buf);

    return rc;
}