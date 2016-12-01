#ifndef _SERVO_UTIL_H_
#define _SERVO_UTIL_H_

#include <stdio.h>
#include <ctype.h>

#include <kore/kore.h>
#include <kore/http.h>

#include <jansson.h>
#include "assets.h"

int servo_read_cookie(struct http_request *req, const char *cookie, char **value);

int servo_response(struct http_request * req, const int http_code, struct kore_buf *buf);

int servo_response_html(struct http_request * req, const int http_code,
                                              const void* asset_html,
                                              const size_t asset_len_html);

int servo_response_json(struct http_request * req, const int http_code, const json_t *json);

int servo_response_error(struct http_request *req, const int http_code, const char* err);

#endif //_SERVO_UTIL_H_
