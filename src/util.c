#include "util.h"
#include "assets.h"
#include "servo.h"
#include "ini.h"

char   *servo_config_paths[] = {
    "$HOME/.servo/conf",
    "$PREFIX/conf/servo.conf"
};
#define servo_config_paths_size (sizeof(servo_config_paths) / sizeof(servo_config_paths[0]))

#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

static int servo_read_config_handler(void* user, const char* section, const char* name,
    const char* value)
{
    struct servo_config *cfg;

    cfg = (struct servo_config *)user;
    if (MATCH("servo", "public_mode")) {
        cfg->public_mode = atoi(value);
    } else if (MATCH("servo", "database")) {
        cfg->database = kore_strdup(value);
    } else if (MATCH("session", "ttl")) {
        cfg->session_ttl = atoi(value);
    } else if (MATCH("session", "string_size")) {
        cfg->string_size = atoi(value);
    } else if (MATCH("session", "json_size")) {
        cfg->json_size = atoi(value);
    } else if (MATCH("session", "blob_size")) {
        cfg->blob_size = atoi(value);
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

static int
servo_parse_config(const char* path, struct servo_config *cfg)
{
    struct stat      st;

    if (stat(path, &st) != 0) {
        return (KORE_RESULT_ERROR);
    }

    if (ini_parse(path, servo_read_config_handler, cfg) < 0) {
        kore_log(LOG_ERR, "failed to parse configuration.");
        return (KORE_RESULT_ERROR);
    }

    return (KORE_RESULT_OK);
}

int servo_read_config(struct servo_config *cfg)
{
    
    char                *p, *path;
    char                 home[PATH_MAX], prefix[PATH_MAX];
    char                *homevar;
    struct kore_buf     *buf;
    int                  parsed;
    size_t               i;
    
    parsed = 0;
    /* $HOME defaults to "." */
    homevar = getenv("HOME");
    if (homevar == NULL || strlen(homevar) == 0)
        homevar = ".";

    memset(prefix, 0, sizeof(PATH_MAX));
    memset(home, 0, sizeof(PATH_MAX));
    strcpy(home, homevar);

#if defined(PREFIX)
    strcpy(prefix, PREFIX);
#else
    strcpy(prefix, "/usr/local/servo");
#endif

    for(i = 0; i < servo_config_paths_size; ++i) {
        p = servo_config_paths[i];
        
        /* read config paths and replace supported variables:
         * $HOME => env variable
         * $PREFIX => "-DPREFIX" value or "/usr/local/servo" default
         */
        buf = kore_buf_alloc(PATH_MAX);
        kore_buf_append(buf, p, strlen(p));
        if (strstr(p, "$HOME") != NULL) {
            kore_buf_replace_string(buf, "$HOME", home, strlen(home));
        }
        if (strstr(p, "$PREFIX") != NULL) {
            kore_buf_replace_string(buf, "$PREFIX", prefix, strlen(prefix));
        }
        path = kore_buf_stringify(buf, NULL);
        parsed = servo_parse_config(path, cfg);
        kore_buf_free(buf);

        if (parsed) {
            kore_log(LOG_DEBUG, "using \"%s\"", path);
            break;
        }
    }

    if (!parsed)
        return (KORE_RESULT_ERROR);

    return (KORE_RESULT_OK);
}

void servo_response_json(struct http_request * req,
	       	const unsigned int http_code,
		   	const json_t *data)
{
    struct kore_buf *buf;
    char *json;

    buf = kore_buf_alloc(http_body_max);
    json = json_dumps(data, JSON_ENCODE_ANY);
    kore_buf_append(buf, json, strlen(json));

    http_response_header(req, "content-type", "application/json");
    http_response(req, http_code, buf->data, buf->offset);
    kore_buf_free(buf);
    free(json);
}

void
servo_response_error(struct http_request *req,
			const unsigned int http_code,
			const char* err)
{
    json_t* data;
    
    data = json_pack("{s:i s:s}", "code", http_code, "error", err);
    servo_response_json(req, http_code, data);
    json_decref(data);
}

struct kore_buf *
servo_request_data(struct http_request *req)
{
    struct kore_buf     *buf;
    int                  r;
    char                 data[BUFSIZ];


    buf = kore_buf_alloc(http_body_max);

    for (;;) {
        r = http_body_read(req, data, sizeof(data));
        if (r == -1) {
            kore_buf_free(buf);
            return NULL;
        }
        if (r == 0)
            break;
        kore_buf_append(buf, data, r);
    }

    return buf;
}

int
servo_is_item_request(struct http_request *req)
{
    return (strcmp(req->path, ROOT_PATH) != 0 &&
            strcmp(req->path, CONSOLE_JS_PATH) != 0);
}