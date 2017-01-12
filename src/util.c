#include "util.h"
#include "assets.h"
#include "servo.h"

static char *g_servo_config = "/home/stan/.servo/config.json";

static void servo_log_jerr(const char* fname, json_error_t *jerr)
{
    kore_log(LOG_ERR, "%s: %s.\n\tline: %d, column: %d, position: %d",
             fname, jerr->text, jerr->line, jerr->column, jerr->position);
}

int servo_read_config(struct servo_config *cfg)
{
    json_error_t jerr;
    json_t *json;

    json = json_load_file(g_servo_config, JSON_ALLOW_NUL, &jerr);
    if (json == NULL && jerr.text && strlen(jerr.text)) {
        servo_log_jerr(__FUNCTION__, &jerr);
        return (KORE_RESULT_ERROR);
    } 

    /* Config Parameters: 
     * "db" (required)     - [string] database connection
     * "public_mode"       - [boolean] enable public mode
     * "session_ttl"       - [int] session timeout in seconds
     * "string_value_size" - [int] size of 'varchar' values column
     * "json_value_size"   - [int] size of 'json' values column
     * "blob_value_size"   - [int] size of 'bytea' values column
     * "allow_origin"      - [string] enable 'Origin' header filtering and match value
     * "allow_ipaddr"      - [string] enable client ip address filtering and match value
     */ 
    if (json_unpack_ex(json, &jerr, 0, "{s:s s?:b s?:i s?:i s?:i s?:i s?:i s?:s}",
                       "db", &cfg->connect,
                       "public_mode", &cfg->public_mode,
                       "session_ttl", &cfg->session_ttl,
                       "max_sessions", &cfg->max_sessions,
                       "string_value_size", &cfg->val_string_size,
                       "json_value_size", &cfg->val_json_size,
                       "blob_value_size", &cfg->val_blob_size,
                       "allow_origin", &cfg->allow_origin,
                       "allow_ipaddr", &cfg->allow_ipaddr) != 0) {
        servo_log_jerr(__FUNCTION__, &jerr);
        return (KORE_RESULT_ERROR);
    }
    return (KORE_RESULT_OK);
}

void servo_response_cookie(struct http_request *req, const char* name, const char* val)
{
    char                     cookie[BUFSIZ];

    memset(cookie, 0, sizeof(cookie));
    snprintf(cookie, BUFSIZ - 1, "%s=%s;", name, val);
    http_response_header(req, "set-cookie", cookie);
}

int servo_response(struct http_request * req,
		   const unsigned int http_code,
		   struct kore_buf *buf)
{
    http_response(req, http_code, buf->data, buf->offset);
    return (KORE_RESULT_OK);
}

int servo_response_json(struct http_request * req,
	       	const unsigned int http_code,
		   	const json_t *data)
{
    int rc;
    struct kore_buf *buf;
    char *json;

    buf = kore_buf_alloc(2048);
    json = json_dumps(data, JSON_ENCODE_ANY);
    kore_buf_append(buf, json, strlen(json));

    http_response_header(req, "content-type", "application/json");
    rc = servo_response(req, http_code, buf);
    kore_buf_free(buf);
    free(json);
    return rc;
}

int servo_response_error(struct http_request *req,
			const unsigned int http_code,
			const char* err)
{
    int rc;
    json_t* data;
    
    data = json_pack("{s:i s:s}", "code", http_code, "error", err);
    rc = servo_response_json(req, http_code, data);
    json_decref(data);
    return rc;
}

char * servo_request_str_data(struct http_request *req)
{
    static char data[BUFSIZ];
    int rc = KORE_RESULT_OK;

    rc = http_body_read(req, data, sizeof(data));
    if (rc != KORE_RESULT_OK) {
        kore_log(LOG_ERR, "%s: failed to read request body",
                 __FUNCTION__);
        return NULL;
    }

    return data;
}

json_t * servo_request_json_data(struct http_request *req)
{
    json_error_t jerr;
    json_t *json = NULL;
    char *str = NULL;

    str = servo_request_str_data(req);
    if (str == NULL) {
        return NULL;
    }

    json = json_loads(str, JSON_ALLOW_NUL, &jerr);
    if (json == NULL) {
        servo_log_jerr(__FUNCTION__, &jerr);
        return NULL;
    }

    return NULL;
}