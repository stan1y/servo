#include <kore/kore.h>
#include <kore/http.h>
#include <jansson.h>
#include "assets.h"

const char*
get_client_ipaddr(struct http_request *req);

size_t
populate_api_arguments(struct http_request *req);

int
response_with(struct http_request * req, const int http_code, struct kore_buf *buf);

int
response_with_html(struct http_request * req, const int http_code,
                                              const void* asset_html,
                                              const size_t asset_len_html);

int 
response_with_json(struct http_request * req, const int http_code, const json_t *json);