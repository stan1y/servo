#include "util.h"

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int servo_read_cookie(struct http_request *req, const char *name, char **out)
{
    int	i, v;
    size_t	len, slen;
    char	*value, *c, *cookie, *cookies[HTTP_MAX_COOKIES];

    if (!http_request_header(req, "cookie", &c))
	return (KORE_RESULT_ERROR);

    cookie = kore_strdup(c);

    slen = strlen(name);
    v = kore_split_string(cookie, ";", cookies, HTTP_MAX_COOKIES);
    for (i = 0; i < v; i++) {
	for (c = cookies[i]; isspace(*c); c++)
	    ;

	len = MIN(slen, strlen(cookies[i]));
	if (!strncmp(c, name, len))
	    break;
    }

    if (i == v) {
	kore_free(cookie);
	return (KORE_RESULT_ERROR);
    }

    c = cookies[i];
    if ((value = strchr(c, '=')) == NULL) {
	kore_free(cookie);
	return (KORE_RESULT_ERROR);
    }
    
    ++value;
    if (value)
        *out = kore_strdup(value);
    kore_free(cookie);

    return (i);
}

int servo_response(struct http_request * req,
		   const int http_code,
		   struct kore_buf *buf)
{
    http_response(req, http_code, buf->data, buf->offset);
    kore_log(LOG_NOTICE, "%s: code=%d, wrote %d bytes",
	     __FUNCTION__, http_code, buf->offset);
    return (KORE_RESULT_OK);
}

int servo_response_html(struct http_request * req,
			const int http_code,
                        const void* asset_html,
                        const size_t asset_len_html)
{
    struct kore_buf *buf;
    int rc;

    buf = kore_buf_alloc(asset_len_html);
    kore_buf_append(buf, asset_html, asset_len_html);
    http_response_header(req, "content-type", "text/html");
    rc = servo_response(req, http_code, buf);
    kore_buf_free(buf);

    return rc;
}

int servo_response_json(struct http_request * req,
	       	   	const int http_code,
		   	const json_t *data)
{
    int rc;
    struct kore_buf *buf;
    char *json;

    buf = kore_buf_alloc(2048);
    json = json_dumps(data, JSON_ENCODE_ANY);
    kore_buf_append(buf, json, strlen(json));

    http_response_header(req, "Content-Type", "application/json");
    rc = servo_response(req, http_code, buf);
    kore_buf_free(buf);
    free(json);
    return rc;
}

int servo_response_error(struct http_request *req,
			 const int http_code,
			const char* err)
{
    int rc;
    json_t* data;
    
    data = json_pack("{s:i s:s}", "code", http_code, "error", err);
    rc = servo_response_json(req, http_code, data);
    json_decref(data);
    return rc;
}
