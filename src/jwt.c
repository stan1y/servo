#include <jansson.h>
#include "jwt.h"
#include "servo.h"


unsigned char* hmac_sha256(const void *key, int keylen,
                           const unsigned char *data, int datalen,
                           unsigned char *result, unsigned int* resultlen)
{
    return HMAC(EVP_sha256(), key, keylen, data, datalen, result, resultlen);
}

struct json_web_token *
jwt_parse(char *s)
{
    struct json_web_token   *jwt;
    char                    *str, *parts[JWT_MAX_SIZE];
    int                      v;
    u_int8_t				 header[JWT_MAX_SIZE],
    						 payload[JWT_MAX_SIZE],
    						 sign[JWT_MAX_SIZE];
    size_t                   hl = JWT_MAX_SIZE,
                             pl = JWT_MAX_SIZE,
                             sl = JWT_MAX_SIZE;

    str = kore_strdup(s);
    v = kore_split_string(str, ".", parts, 3);
    if (v != 3) {
        kore_log(LOG_ERR, "%s: invalid json web token format.",
                 __FUNCTION__);
        return NULL;
    }

    jwt = kore_malloc(sizeof(struct json_web_token));
    if (!kore_base64_decode(parts[0], (u_int8_t**)&header, &hl)) {
    	kore_log(LOG_ERR, "failed to decode jwt header");
    	goto error;
    }
    if (!kore_base64_decode(parts[1], (u_int8_t**)&payload, &pl)) {
    	kore_log(LOG_ERR, "failed to decode jwt payload");
    	goto error;
    }
    if (!kore_base64_decode(parts[2], (u_int8_t**)&sign, &sl)) {
    	kore_log(LOG_ERR, "failed to decode jwt header");
    	goto error;
    }
    kore_log(LOG_DEBUG, "JWT header: %s", header);
    kore_log(LOG_DEBUG, "JWT sign: %s", sign);
    kore_log(LOG_DEBUG, "JWT payload: %s", payload);

    return jwt;

error:
	kore_free(jwt);
	return NULL;
}

struct
kore_buf *
jwt_build(const char* alg, char *payload, size_t len)
{
	struct kore_buf		*jwtbuf;
  json_t				    *header;
  char				      *sheader,
                    *eheader, *epayload, *esign,
                    *raw_token;
  uint8_t				    sign[JWT_MAX_SIGNATURE];
  unsigned int		  slen = JWT_MAX_SIGNATURE;

	if (strcmp(alg, JWT_ALG_HS256) != 0) {
		kore_log(LOG_ERR, "invalid jwt algorithm: %s", alg);
		return NULL;
	}
	jwtbuf = kore_buf_alloc(JWT_MAX_SIZE);
  // encode header
	header = json_pack("{s:s s:s}",
					   "alg", alg,
					   "typ", "jwt");

	sheader = json_dumps(header, 0);
	json_decref(header);
  kore_base64_encode((u_int8_t*)sheader, strlen(sheader), &eheader);
  kore_buf_append(jwtbuf, eheader, strlen(eheader));
  free(sheader);

  // encode payload
	kore_buf_append(jwtbuf, ".", 1);
  kore_base64_encode((u_int8_t*)payload, len, &epayload);
  kore_buf_append(jwtbuf, epayload, strlen(epayload));

	// calc & encode signature
  raw_token = kore_buf_stringify(jwtbuf, NULL);
	hmac_sha256(
		CONFIG->jwt_key, strlen(CONFIG->jwt_key),
		(const unsigned char *)raw_token, strlen(raw_token),
		sign, &slen);
  kore_base64_encode(sign, slen, &esign);

  kore_buf_free(jwtbuf);
  jwtbuf = kore_buf_alloc(JWT_MAX_SIZE);
  kore_buf_append(jwtbuf, eheader, strlen(eheader));
  kore_buf_append(jwtbuf, ".", 1);
  kore_buf_append(jwtbuf, epayload, strlen(epayload));
  kore_buf_append(jwtbuf, ".", 1);
  kore_buf_append(jwtbuf, esign, strlen(esign));

	return jwtbuf;
}
