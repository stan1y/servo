#ifndef _SERVO_UTIL_H_
#define _SERVO_UTIL_H_

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <limits.h>

#include <kore/kore.h>
#include <kore/http.h>

#include <jansson.h>

#define CONTENT_TYPE_STRING		"text/plain; charset=UTF-8"
#define CONTENT_TYPE_JSON		"application/json; charset=UTF-8"
#define CONTENT_TYPE_BLOB		"multipart/form-data; charset=UTF-8"
#define CONTENT_TYPE_HTML		"text/html; charset=UTF-8"

#define SERVO_CONTENT_STRING	0
#define SERVO_CONTENT_JSON		1
#define SERVO_CONTENT_BLOB		2

int					 servo_is_item_request(struct http_request *);
struct kore_buf		*servo_request_data(struct http_request *);
void				 servo_request_content_types(struct http_request *);
void				 servo_response_json(struct http_request *,
						const unsigned int, const json_t *);
void				 servo_response_status(struct http_request *,
						const unsigned int, const char *);
void				 servo_handle_pg_error(struct http_request *);

char        *servo_random_string(char *, size_t);
#endif //_SERVO_UTIL_H_
