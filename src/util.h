#ifndef _SERVO_UTIL_H_
#define _SERVO_UTIL_H_

#include <stdio.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <kore/kore.h>
#include <kore/http.h>

#include <jansson.h>

#define SERVO_CONTENT_STRING 0
#define SERVO_CONTENT_JSON   1
#define SERVO_CONTENT_BLOB   2

char * servo_request_str_data(struct http_request *req);
json_t * servo_request_json_data(struct http_request *req);
void * servo_request_blob_data(struct http_request *req);

int servo_read_cookie(struct http_request *req, const char *cookie, char* value);

int servo_response(struct http_request * req, const int http_code, struct kore_buf *buf);

int servo_response_html(struct http_request * req, const int http_code,
                                              const void* asset_html,
                                              const size_t asset_len_html);

int servo_response_json(struct http_request * req, const int http_code, const json_t *json);

int servo_response_error(struct http_request *req, const int http_code, const char* err);

#endif //_SERVO_UTIL_H_
