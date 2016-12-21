#include "servo.h"

int servo_render_stats(struct http_request *req, struct servo_context *ctx)
{
    int			 rc;
    char		 expire_on[80];
    json_t		 *stats;

    strftime(expire_on, sizeof(expire_on), "%a %Y-%m-%d %H:%M:%S %Z", &ctx->session.expire);
    
    stats = json_pack("{s:s s:s}",
		      "client", ctx->session.client,
		      "expire_on", expire_on);
    rc = servo_response_json(req, 200, stats);
    json_decref(stats);
    return rc;
}

int servo_render_console(struct http_request *req, struct servo_context *ctx)
{

    return (KORE_RESULT_OK);
}