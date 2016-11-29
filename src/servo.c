#include "servo.h"
#include "util.h"

int
reposnse_with_error(struct http_request *req, const int http_code, const char *err);

int
response_with_data(struct http_request *req, const int http_code, const json_t *data);


static char* JSON_TYPE = "application/json";
static char*  XML_TYPE = "application/xml";
static char* HTML_TYPE = "html";

int
response_with_data(struct http_request *req, const int http_code, const json_t *data)
{
    char *accept;
    struct kore_buf *buf;

    // default to plain text
    if (!http_request_header(req, "accept", &accept))
        accept = "application/json";
    kore_log(LOG_NOTICE, "writing data as %s", accept);

    buf = kore_buf_alloc(2048);

    if (strstr(accept, JSON_TYPE) != NULL) {
        char *json = json_dumps(data, JSON_ENCODE_ANY);
        kore_buf_append(buf, json, strlen(json));
    }

    if (strstr(accept, XML_TYPE) != NULL) {
    }

    if (strstr(accept, HTML_TYPE) != NULL) {
        static char *formatted = "{'asd': 1}";
        kore_buf_append(buf, asset_success_html, asset_len_success_html);
        kore_buf_replace_string(buf, "$data$", formatted, strlen(formatted));
    }
    kore_log(LOG_NOTICE, "failed to generate response with data of accepted type: %s",
                         accept);
    return (KORE_RESULT_ERROR);
}

/**
 * Get or create new session based on current settings
 * May raise 403 error if requetor is forbidden for any reason
 */
int
handle_session(struct http_request *req)
{
    size_t args;
    json_t *resp;
    const char *ipaddr;
    
    if (req->method != HTTP_METHOD_GET) {
        kore_log(LOG_NOTICE, "Unexpected method received in session handler.");
        return response_with_html(req, 400, asset_error_html, asset_len_error_html);
    }
    
    // check ip is not in black list
    ipaddr = get_client_ipaddr(req);
    
    args = populate_api_arguments(req);
    kore_log(LOG_NOTICE, "request from %s with %d args", ipaddr, args);

    resp = json_object();
    json_object_set(resp, "key", json_string("value"));
    json_object_set(resp, "key2", json_integer(123));
    if (!response_with_data(req, 200, resp)) {
        return (KORE_RESULT_ERROR);
    }

    json_decref(resp);
    return (KORE_RESULT_OK);
}

int
handle_list(struct http_request *req)
{
    return response_with_html(req, 200, asset_success_html, asset_len_success_html);
}

int
handle_item(struct http_request *req)
{
    return response_with_html(req, 200, asset_success_html, asset_len_success_html);
}
