#ifndef _SERVO_H_
#define _SERVO_H_

#include <uuid/uuid.h>

#include <kore/kore.h>
#include <kore/http.h>
#include <kore/pgsql.h>
#include <jansson.h>

#include "util.h"

#define CLIENT_LEN 255
#define ITEM_KEY_LEN 255

#define PGSQL_FORMAT_TEXT 0
#define PGSQL_FORMAT_BINARY 1

struct servo_session {

    char		 client[CLIENT_LEN];
    time_t		 expire_on;
};

struct servo_config {

	char		*database;
	int			 public_mode;
	size_t		 session_ttl;
	size_t		 max_sessions;

	/* values size limits */
	size_t		 string_size;
	size_t		 json_size;
	size_t		 blob_size;

	/* filtering */
	char		 *allow_origin;
	char		 *allow_ipaddr;
};

extern struct servo_config *CONFIG;

struct servo_context {

	int					 status;
    struct kore_pgsql	 sql;
    struct servo_session session;

    /* in/out content-type */
    int		 in_content_type;
    int		 out_content_type;

    /* item data */
    char	*val_str;
    json_t	*val_json;
    void	*val_blob;
    size_t	 val_sz;
};

int						 servo_read_config(struct servo_config *cfg);

struct servo_context	*servo_create_context(struct http_request *req);
int						 servo_put_session(struct servo_session *s);

int						 servo_init(int state);
int						 servo_start(struct http_request *);
int						 servo_render_stats(struct http_request *req);
int						 servo_render_console(struct http_request *req);

char					*servo_item_to_string(struct servo_context *);
char					*servo_item_to_json(struct servo_context *);

int						 servo_is_success(struct servo_context *);
int						 servo_is_redirect(struct servo_context *);


/* States */

#define REQ_STATE_C_SESSION     0
#define REQ_STATE_Q_SESSION     1
#define REQ_STATE_W_SESSION     2
#define REQ_STATE_R_SESSION     3
#define REQ_STATE_C_ITEM        4
#define REQ_STATE_Q_ITEM        5
#define REQ_STATE_W_ITEM        6
#define REQ_STATE_R_ITEM        7
#define REQ_STATE_ERROR         8
#define REQ_STATE_DONE          9

int					 state_connect_session(struct http_request *);
int				 	 state_query_session(struct http_request *);
int					 state_wait_session(struct http_request *);
int					 state_read_session(struct http_request *);
					
int					 state_connect_item(struct http_request *);
int					 state_query_item(struct http_request *);
int					 state_wait_item(struct http_request *);
int					 state_read_item(struct http_request *);
int					 state_error(struct http_request *);
int					 state_done(struct http_request *);
					
int					 servo_connect_db(struct http_request *, int, int, int);
int					 servo_wait(struct http_request *, int, int, int);

const char 			*servo_state(int s);
const char			*servo_sql_state(int s);
const char			*servo_request_state(struct http_request *);

#endif //_SERVO_H_
