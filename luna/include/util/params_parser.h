#ifndef PARAMS_PARSER_H
#define PARAMS_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rest_api.h"

int parse_params(param_t **out, const char *body, int body_len);
void debug_log_params(param_t *params, int indentation);
void free_params(param_t **params);

#ifdef __cplusplus
}
#endif

#endif
