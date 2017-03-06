#ifndef HEADERS_PARSER
#define HEADERS_PARSER

#ifdef __cplusplus
extern "C" {
#endif

#include "rest_api.h"

int parse_headers(header_t **headers, const char *headers_and_body, int len);
void free_headers(header_t **headers);

#ifdef __cplusplus
}
#endif

#endif
