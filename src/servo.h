#include <kore/kore.h>
#include <kore/http.h>
#include <jansson.h>

/**
 * Get or create new session based on current settings
 * May raise 403 error if requetor is forbidden for any reason
 */
int
handle_session(struct http_request *req);

/**
 * Get list of keys stored in current session storage
 */
int
handle_list(struct http_request *req);

/**
 * Get, Put and Delete a value with given key in session storage
 * Raises 400 error if input data is missing values
 */
int
handle_item(struct http_request *req);
