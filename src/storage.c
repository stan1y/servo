#include "storage.h"
#include "session.h"
#include "util.h"

#define MAX_ITEM_KEY 2048

int servo_storage_get(struct http_request *req)
{
    struct servo_session *s;
    const char *ipaddr;
    char item_key[MAX_ITEM_KEY];
    
    s = servo_get_session(req);
    ipaddr = servo_get_client_ipaddr(s);
    
    kore_log(LOG_NOTICE, "GET {%s} -> %s", item_key, ipaddr);
    
    return (KORE_RESULT_OK);
}

int servo_storage_post(struct http_request *req)
{
    struct servo_session *s;
    const char *ipaddr;
    char item_key[MAX_ITEM_KEY];

    s = servo_get_session(req);
    ipaddr = servo_get_client_ipaddr(s);

    kore_log(LOG_NOTICE, "POST {%s} <- %s", item_key, ipaddr);

    return (KORE_RESULT_OK);
}

int servo_storage_put(struct http_request *req)
{
    struct servo_session *s;
    const char *ipaddr;
    char item_key[MAX_ITEM_KEY];

    s = servo_get_session(req);
    ipaddr = servo_get_client_ipaddr(s);

    kore_log(LOG_NOTICE, "PUT {%s} <- %s", item_key, ipaddr);

    return (KORE_RESULT_OK);
}

int servo_storage_delete(struct http_request *req)
{
    struct servo_session *s;
    const char *ipaddr;
    char item_key[MAX_ITEM_KEY];

    s = servo_get_session(req);
    ipaddr = servo_get_client_ipaddr(s);

    kore_log(LOG_NOTICE, "DELETE {%s} <- %s", item_key, ipaddr);

    return (KORE_RESULT_OK);
}
