#ifndef _SERVO_UTIL_H_
#define _SERVO_UTIL_H_

#include <stdio.h>
#include <ctype.h>
#include <unistd.h>

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <linux/limits.h>

#include <kore/kore.h>
#include <kore/http.h>

#include <jansson.h>

#define SERVO_CONTENT_STRING 0
#define SERVO_CONTENT_JSON   1
#define SERVO_CONTENT_BLOB   2

char		*servo_request_str_data(struct http_request *);
json_t		*servo_request_json_data(struct http_request *);

void		servo_response_json(struct http_request *,
				const unsigned int, const json_t *);
void		servo_response_error(struct http_request *,
				const unsigned int, const char *);

#endif //_SERVO_UTIL_H_
