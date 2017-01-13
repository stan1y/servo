#include "util.h"
#include "assets.h"
#include "servo.h"
#include "ini.h"

static char *g_servo_config = "%s/.servo/config";

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

static int servo_read_config_handler(void* user, const char* section, const char* name,
    const char* value)
{
    struct servo_config *cfg;

    cfg = (struct servo_config *)user;
    if (MATCH("servo", "public_mode")) {
        cfg->public_mode = atoi(value);
    } else if (MATCH("servo", "session_ttl")) {
        cfg->session_ttl = atoi(value);
    } else if (MATCH("servo", "database")) {
        cfg->connect = kore_strdup(value);
    } else if (MATCH("filter", "origin")) {
        cfg->allow_origin = kore_strdup(value);
    } else if (MATCH("filter", "ip_address")) {
        cfg->allow_ipaddr = kore_strdup(value);
    }
    else {
        kore_log(LOG_ERR, "unknown option \"%s.%s\"",
            section, name);
    }

    return 1;
}

int servo_read_config(struct servo_config *cfg)
{
    struct stat      st;
    char             path[PATH_MAX];
    char            *home;
    
    home = getenv("HOME");
    if (home == NULL || strlen(home) == 0)
        home = ".";

    snprintf(path, PATH_MAX, g_servo_config, home);
    kore_log(LOG_DEBUG, "reading %s", path);

    if (stat(path, &st) != 0) {
        kore_log(LOG_ERR, "no configuration file found at %s",
            path);
        return (KORE_RESULT_ERROR);
    }

    if (ini_parse(path, servo_read_config_handler, cfg) < 0) {
        kore_log(LOG_ERR, "failed to parse configuration.");
        return (KORE_RESULT_ERROR);
    }
    return (KORE_RESULT_OK);
}

void servo_response_json(struct http_request * req,
	       	const unsigned int http_code,
		   	const json_t *data)
{
    struct kore_buf *buf;
    char *json;

    buf = kore_buf_alloc(2048);
    json = json_dumps(data, JSON_ENCODE_ANY);
    kore_buf_append(buf, json, strlen(json));

    http_response_header(req, "content-type", "application/json");
    http_response(req, http_code, buf->data, buf->offset);
    kore_buf_free(buf);
    free(json);
}

void servo_response_error(struct http_request *req,
			const unsigned int http_code,
			const char* err)
{
    json_t* data;
    
    data = json_pack("{s:i s:s}", "code", http_code, "error", err);
    servo_response_json(req, http_code, data);
    json_decref(data);
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

static void
servo_log_jerr(const char* fname, const json_error_t *jerr)
{
    kore_log(LOG_ERR, "%s: %s.\n\tline: %d, column: %d, position: %d",
             fname, jerr->text, jerr->line, jerr->column, jerr->position);
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