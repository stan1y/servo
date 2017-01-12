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

char		*servo_request_str_data(struct http_request *);
json_t		*servo_request_json_data(struct http_request *);

int			 servo_request_cookie(struct http_request *, const char *, char **);
void		 servo_response_cookie(struct http_request *, const char *, const char *);

int			servo_response(struct http_request *,
				const unsigned int, struct kore_buf *);
int			servo_response_json(struct http_request *,
				const unsigned int, const json_t *);
int			servo_response_error(struct http_request *,
				const unsigned int, const char *);

#endif //_SERVO_UTIL_H_
