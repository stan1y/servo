#include <kore/kore.h>
#include <kore/http.h>

#include "assets.h"

int
response_with(struct http_request * req, const int http_code, struct kore_buf *buf);

int
response_with_html(struct http_request * req, const int http_code,
											const void* asset_html,
											const size_t asset_len_html);

int
handle_selected_photos(struct http_request *req);


/**
  * Utilities
  */

int
response_with(struct http_request * req, const int http_code, struct kore_buf *buf)
{
	http_response_header(req, "content-type", "text/html");
	http_response(req, http_code, buf->data, buf->offset);
	kore_log(LOG_NOTICE, "responsed with code %d, wrote %d bytes", http_code, buf->offset);
	return (KORE_RESULT_OK);
}

int
response_with_html(struct http_request * req, const int http_code,
											const void* asset_html,
											const size_t asset_len_html)
{
	struct kore_buf *buf;
	int rc;

	buf = kore_buf_alloc(asset_len_html);
	kore_buf_append(buf, asset_html, asset_len_html);
	rc = response_with(req, http_code, buf);
	kore_buf_free(buf);

	return rc;
}

/**
  * Photo selection handler
  */

int
handle_selected_photos(struct http_request *req)
{
	size_t				args;
	struct http_arg		*q, *qnext;

	if (req->method == HTTP_METHOD_POST) {
		args = http_populate_post(req);
	}
	else {
		kore_log(LOG_NOTICE, "Unexpected method received.");
		return response_with_html(req, 400, asset_error_html, asset_len_error_html);
	}

	// read selected photos
	kore_log(LOG_NOTICE, "select photos: %d arguments received", args);
	for (q = TAILQ_FIRST(&(req->arguments)); q != NULL; q = qnext) {
		qnext = TAILQ_NEXT(q, list);
		kore_log(LOG_NOTICE, "- %s=%s", q->name, q->s_value);
	}

	// response with success message
	return response_with_html(req, 200, asset_success_html, asset_len_success_html);
}
