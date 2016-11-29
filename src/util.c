#include "util.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
  * Utilities
  */
  
const char*
get_client_ipaddr(struct http_request *req)
{
    static char ipaddr[INET_ADDRSTRLEN];
    memset(ipaddr, 0, sizeof(char) * INET_ADDRSTRLEN);
    
    switch (req->owner->addrtype) {
    case AF_INET:
        /* IP is under connection->addr.ipv4 */
        inet_ntop(AF_INET, &(req->owner->addr.ipv4), ipaddr, INET_ADDRSTRLEN);
        break;
    case AF_INET6:
        /* IP is under connection->addr.ipv6 */
        inet_ntop(AF_INET6, &(req->owner->addr.ipv6), ipaddr, INET_ADDRSTRLEN);
        break;
    }
    
    return ipaddr;
}
  
  
size_t
populate_api_arguments(struct http_request *req)
{
    char * content_type;
    
    http_request_header(req, "content-type", &content_type);
    kore_log(LOG_NOTICE, "reading content type: %s", content_type);
    
    if (strstr(content_type, "application/json") != NULL) {
        // read json body
        return 0;
    }
    
    if (strstr(content_type, "www-form-encoded") != NULL) {
        if (req->method == HTTP_METHOD_POST)
            return http_populate_post(req);
        if (req->method == HTTP_METHOD_GET)
            return http_populate_get(req);
    }
    
    if (strstr(content_type, "multipart") != NULL) {
        return http_populate_multipart_form(req);
    }
    
    kore_log(LOG_NOTICE, "no arguments read  - failed to understand: %s",
             content_type);
    return 0;
}

int
response_with(struct http_request * req, const int http_code, struct kore_buf *buf)
{
    http_response_header(req, "content-type", "text/html");
    http_response(req, http_code, buf->data, buf->offset);
    kore_log(LOG_NOTICE, "responsed with code %d, wrote %d bytes", http_code, buf->offset);
    return (KORE_RESULT_OK);
}

int
response_with_html(struct http_request * req, const int http_code,
                                              const void* asset_html,
                                              const size_t asset_len_html)
{
    struct kore_buf *buf;
    int rc;

    buf = kore_buf_alloc(asset_len_html);
    kore_buf_append(buf, asset_html, asset_len_html);
    rc = response_with(req, http_code, buf);
    kore_buf_free(buf);

    return rc;
}

int
response_with_json(struct http_request * req, const int http_code, const json_t *json)
{
    return KORE_RESULT_OK;
}