#include "servo.h"
#include "util.h"

/**
 * Bootstrap servo service on start
 */
int
servo_startup(int state);

/**
 * Handle GET call to the root (index).
 * Index request can produce stats as json 
 * or built-in session console in `public` mode
 */
int
servo_session_index(struct http_request *req);

int
servo_render_stats(struct http_request *req);


struct servo_context *
servo_init(void)
{
    struct servo_context *ctx;
    char *ttlstr;
    int ttl;

    ttlstr = getenv("SERVO_TTL");
    if (ttlstr == NULL)
	ttlstr = "300";
    ttl = atoi(ttlstr);

    ctx = kore_malloc(sizeof(struct servo_context));
    TAILQ_INIT(&ctx->sessions);
    ctx->session_ttl = ttl;

    kore_log(LOG_NOTICE, "new context created, session_ttl=%d", ctx->session_ttl);
 
    return ctx;
}

int
servo_startup(int state)
{
    char *connstr;

    connstr = getenv("SERVO_DB");
    if (connstr == NULL)
        connstr = "postgresql://servo@localhost";

    kore_log(LOG_NOTICE, "using database at \"%s\"", connstr);
    kore_pgsql_register("db", connstr);
    
    return (KORE_RESULT_OK);
}
    
int
servo_render_stats(struct http_request *req)
{
    int			 rc;
    const char		 *ipaddr;
    char		 expire_at[80];
    json_t		 *stats;
    struct servo_session *s;

    s = servo_get_session(req);
    ipaddr = servo_get_client_ipaddr(s);
    strftime(expire_at, sizeof(expire_at), "%a %Y-%m-%d %H:%M:%S %Z", &s->expire_at);
    
    stats = json_pack("{s:s s:i s:s}",
		      "client", ipaddr,
		      "ttl", s->ttl,
		      "expire_at", expire_at);
    rc = servo_response_json(req, 200, stats);
    json_decref(stats);
    return rc;
}

int servo_session_index(struct http_request *req)
{
    char *accept;

    if (!http_request_header(req, "Accept", &accept))
	accept = "application/json";

    if (strstr(accept, "text/html") != NULL)
	return servo_response_html(req, 200,
				   asset_console_html,
				   asset_len_console_html);
    if (strstr(accept, "application/json") != NULL)
	return servo_render_stats(req);

    return (KORE_RESULT_OK);
}

int servo_session(struct http_request *req)
{
    struct servo_context *ctx;

    /* Setup our state context (if not yet set). */
    if (req->hdlr_extra == NULL) {
	ctx = servo_init();
	req->hdlr_extra = ctx;
    } else {
	ctx = req->hdlr_extra;
    }

    switch (req->method) {

    default:
    case HTTP_METHOD_GET:
	if (strlen(req->path))
	    return servo_storage_get(req);
	else
	    return servo_session_index(req);
	break;

    case HTTP_METHOD_POST:
    case HTTP_METHOD_PUT:
    case HTTP_METHOD_DELETE:
	if (strlen(req->path) == 0)
	    return servo_response_error(req, 400,
		    "Invalid operation. Path id is missing");

	if (req->method == HTTP_METHOD_POST)
	    return servo_storage_post(req);

	if (req->method == HTTP_METHOD_PUT)
            return servo_storage_put(req);

	if (req->method == HTTP_METHOD_DELETE)
	    return servo_storage_delete(req);
	break;
    };

    // can not reach
    return (KORE_RESULT_ERROR);
}

int servo_event(struct http_request *req)
{

    return (KORE_RESULT_OK);
}

/*
int 
    size_t args;
    json_t *resp;
    const char *ipaddr;
    
    if (req->method != HTTP_METHOD_GET) {
        kore_log(LOG_NOTICE,
	    "Unexpected method received in session handler.");
        return response_with_html(req, 400,
		asset_error_html, asset_len_error_html);
    }
    
    // check ip is not in black list
    ipaddr = get_client_ipaddr(req);
    
    args = populate_api_arguments(req);
    kore_log(LOG_NOTICE, "request from %s with %d args", ipaddr, args);

    resp = json_object();
    json_object_set(resp, "key", json_string("value"));
    json_object_set(resp, "key2", json_integer(123));
    if (!response_with_json(req, 200, resp)) {
        return (KORE_RESULT_ERROR);
    }

    json_decref(resp);
    return (KORE_RESULT_OK);
}
*/
