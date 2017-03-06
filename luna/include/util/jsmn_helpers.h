/* 
 * File:   json_helpers.h
 * Author: Colin
 *
 * Created on January 20, 2016, 12:23 AM
 */

#ifndef JSON_HELPERS_H
#define	JSON_HELPERS_H

#include "util/jsmn.h"

#ifdef	__cplusplus
extern "C" {
#endif

jsmntok_t *jsmn_skip_node(jsmntok_t *node);
int jsmn_object_iter(const char *json, jsmntok_t *token,
                     int (*iter)(const char *, jsmntok_t *, jsmntok_t *, void *),
                     void *arg);
int jsmn_array_iter(const char *json, jsmntok_t *token,
                    int (*iter)(const char *, jsmntok_t *, void *),
                    void *arg);

#ifdef	__cplusplus
}
#endif

#endif	/* JSON_HELPERS_H */

