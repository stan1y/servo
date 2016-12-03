#include "servo.h"
#include "util.h"

/**
 * Bootstrap servo service on start
 */
int servo_startup(int state);
int servo_session_index(struct http_request *req);
int servo_render_stats(struct http_request *req, struct servo_session *s);

static int	request_init(struct http_request *);
static int	request_session_query(struct http_request *);
static int      request_item_query(struct http_request *);
static int	request_db_wait(struct http_request *);
static int	request_db_read(struct http_request *);
static int	request_error(struct http_request *);
static int	request_done(struct http_request *);

struct http_state   servo_session_states[] = {
    { "REQ_STATE_INIT",		request_init },
    { "REQ_STATE_SESSION",	request_session_query },
    { "REQ_STATE_ITEM",         request_item_query },
    { "REQ_STATE_DB_WAIT",	request_db_wait },
    { "REQ_STATE_DB_READ",	request_db_read },
    { "REQ_STATE_ERROR",	request_error },
    { "REQ_STATE_DONE",		request_done },
};

#define servo_session_states_size (sizeof(servo_session_states) / sizeof(servo_session_states[0]))

struct servo_context *servo_init(void)
{
    struct servo_context *ctx;
    char *ttlstr, *pmode;
    int ttl, public_mode;

    if (SERVO != NULL)
	return SERVO;

    ttlstr = getenv("SERVO_TTL");
    if (ttlstr == NULL)
	ttlstr = "300";
    ttl = atoi(ttlstr);

    pmode = getenv("SERVO_PUBLIC");
    if (pmode == NULL)
	pmode = "0";
    public_mode = atoi(pmode);

    ctx = kore_malloc(sizeof(struct servo_context));
    ctx->ttl = ttl;
    ctx->public_mode = public_mode;
    ctx->session.client = NULL;
    memset(&ctx->session.client, 0, sizeof(char)*CLIENT_LEN);

    kore_log(LOG_NOTICE, "%s: created context - ttl=%d, public=%d",
	    __FUNCTION__, ctx->ttl, ctx->public_mode);
    return ctx;
}

int servo_startup(int state)
{
    char *connstr;

    connstr = getenv("SERVO_DB");
    if (connstr == NULL)
        connstr = "postgresql://servo@localhost";

    kore_log(LOG_NOTICE, "using database at \"%s\"", connstr);
    kore_pgsql_register("db", connstr);
    
    return (KORE_RESULT_OK);
}
    
int servo_render_stats(struct http_request *req, struct servo_session *s)
{
    int			 rc;
    char		 expire_on[80];
    json_t		 *stats;

    strftime(expire_on, sizeof(expire_on), "%a %Y-%m-%d %H:%M:%S %Z", s->expire);
    
    stats = json_pack("{s:s s:i s:s}",
		      "client", s->client,
		      "ttl", s->ttl,
		      "expire_on", expire_on);
    rc = servo_response_json(req, 200, stats);
    json_decref(stats);
    return rc;
}

int servo_session_index(struct http_request *req)
{
    char *accept;
    struct servo_session *s;

    s = servo_get_session(req);

    if (!http_request_header(req, "Accept", &accept))
	accept = "application/json";

    if (strstr(accept, "text/html") != NULL) {
	kore_log(LOG_NOTICE, "serving debug console for client: %s", s->client);
	return servo_response_html(req, 200,
				   asset_console_html,
				   asset_len_console_html);
    }
    if (strstr(accept, "application/json") != NULL) {
	kore_log(LOG_NOTICE, "serving session stats for client: %s", s->client);
	return servo_render_stats(req, s);
    }

    return (KORE_RESULT_OK);
}

int servo_session(struct http_request *req)
{
    kore_log(LOG_NOTICE, "%s: started", __FUNCTION__);
    return (http_state_run(servo_session_states, servo_session_states_size, req));
}

static int request_init(struct http_request *)
{
    struct servo_context *ctx;
    
    kore_log(LOG_NOTICE, "%s: started", __FUNCTION__);
    if (req->hdlr_extra == NULL) {
	cxt = servo_init();
	req->hdlr_extra = ctx;
    } else {
	ctx = req->hdlr_extra;
    }

    if (!http_request_header(req, "Origin", &origin)) {
	kore_log(LOG_ERROR, "no Origin header sent");
	req->fsm_state = REQ_STATE_ERROR;

    }

    if (!kore_pgsql_query_init(&ctx->sql, req, "db", KORE_PGSQL_ASYNC)) {
	/* If the state was still INIT, we'll try again later. */
	if (ctx->sql.state == KORE_PGSQL_STATE_INIT) {
	    req->fsm_state = REQ_STATE_INIT;
	    return (HTTP_STATE_RETRY);
	}

	kore_pgsql_logerror(&state->sql);
	req->fsm_state = REQ_STATE_ERROR;
    } else {
	if (ctx.session.client == NULL)
	    req->fsm_state = REQ_SESSION_QUERY;
	else
	    req->fsm_state = REQ_ITEM_QUERY;
    }

    return (HTTP_STATE_CONTINUE);
}

static int request_session_query(struct http_request *req)
{
    struct servo_context *ctx;

    kore_log(LOG_NOTICE, "%s: started", __FUNCTION__);
    ctx = req->hdlr_extra;
    

    struct servo_context *ctx = NULL;
    char *key = NULL;
    size_t keylen = 0;
    int rc = KORE_RESULT_ERROR;
    static char session_prefix[] = "/session";

    /* Setup our state context (if not yet set). */
    if (req->hdlr_extra == NULL) {
	ctx = servo_init();
	req->hdlr_extra = ctx;
	kore_log(LOG_DEBUG, "set request conext=%p", ctx);
    } else {
	ctx = req->hdlr_extra;
	kore_log(LOG_DEBUG, "use request conext=%p", ctx);
    }

    if (strlen(req->path) > strlen(session_prefix)) {
	keylen = strlen(req->path) - strlen(session_prefix);
	strncpy(key, req->path + strlen(session_prefix), keylen);
	kore_log(LOG_DEBUG, "client requested item key: %s", key);
    }

    switch (req->method) {

    default:
    case HTTP_METHOD_GET:
	if (key && strlen(key))
	    rc = servo_storage_get(req, key);
	else
	    rc = servo_session_index(req);
	break;

    case HTTP_METHOD_POST:
    case HTTP_METHOD_PUT:
    case HTTP_METHOD_DELETE:
	if (!key || strlen(key) == 0)
	    rc = servo_response_error(req, 400,
		    "Invalid operation. Item key is missing");

	if (req->method == HTTP_METHOD_POST)
	    rc = servo_storage_post(req, key);

	if (req->method == HTTP_METHOD_PUT)
            rc = servo_storage_put(req, key);

	if (req->method == HTTP_METHOD_DELETE)
	    rc = servo_storage_delete(req, key);
	break;
    };

    return rc;
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
