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

#include "servo.h"

int                  servo_read_config(struct servo_config *);

int                  servo_is_item_request(struct http_request *);
struct kore_buf     *servo_read_body(struct http_request *);
struct kore_buf     *servo_read_file(struct http_file *);
void                 servo_read_content_types(struct http_request *);

void                 servo_response_json(struct http_request *,
                                         const unsigned int,
                                         const json_t *);
void                 servo_response_status(struct http_request *,
                                           const unsigned int,
                                           const char *);

void                 servo_handle_pg_error(struct http_request *);

char                 *servo_item_to_string(struct servo_context *);
char                 *servo_item_to_json(struct servo_context *);

char                 *servo_random_string(char *, size_t);
char                 *servo_format_date(time_t*);

const char           *servo_state_text(int s);
const char           *sql_state_text(int s);
const char           *servo_request_state(struct http_request *);

int                   servo_connect_db(struct http_request *, int, int, int);
int                   servo_wait(struct http_request *, int, int, int);

int                   servo_is_success(struct servo_context *);
int                   servo_is_redirect(struct servo_context *);

#endif //_SERVO_UTIL_H_
