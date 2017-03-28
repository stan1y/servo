#ifndef _JSON_WEB_TOKEN_H_
#define _JSON_WEB_TOKEN_H_

#include <kore/kore.h>

#define JWT_MAX_SIZE            8192
#define JWT_MAX_SIGNATURE       64

#define JWT_ALG_HS256           "HS256"

struct json_web_token {
    char                *alg;
    struct kore_buf     *payload;
};

unsigned char                   *hmac_sha256(const void *, int,
                                             const unsigned char *, int,
                                             unsigned char *, unsigned int*);
struct json_web_token           *jwt_parse(char *);
struct kore_buf                 *jwt_build(const char *, char *, size_t);

#endif //_JSON_WEB_TOKEN_H_
