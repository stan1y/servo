#include "servo.h"
#include "util.h"

/**
 * Get or create new session based on current settings
 * May raise 403 error if requetor is forbidden for any reason
 */
int
handle_session(struct http_request *req)
{
    size_t				args;
    //struct http_arg		*q, *qnext;
    const char *ipaddr;
    
    if (req->method != HTTP_METHOD_GET) {
        kore_log(LOG_NOTICE, "Unexpected method received in session handler.");
        return response_with_html(req, 400, asset_error_html, asset_len_error_html);
    }
    
    // check ip is not in black list
    ipaddr = get_client_ipaddr(req);
    
    // no args expected
    args = populate_api_arguments(req);
    
    kore_log(LOG_NOTICE, "request from %s with %d args", ipaddr, args);

    return response_with_html(req, 200, asset_success_html, asset_len_success_html);
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