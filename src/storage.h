#ifndef _SERVO_STORE_H_
#define _SERVO_STORE_H_

#include <kore/kore.h>
#include <kore/http.h>
#include <jansson.h> 
#include <hiredis/hiredis.h>

/**
 * Get item from storage by it's key.
 * Query arguments can specify additional options.
 */
int servo_storage_get(struct http_request *);

/**
 * Create a new item with key in storage
 * Item data is specified with Content-Type
 */
int servo_storage_post(struct http_request *);


/**
 * Update an existing item with key
 * Item data is specified with Content-Type
 */
int servo_storage_put(struct http_request *);

/**
 * Remove item with key from storage.
 */
int servo_storage_delete(struct http_request *);

#endif //_SERVO_STORE_H_
