/* 
 * File:   https_api.h
 * Author: Colin
 *
 * Created on December 15, 2015, 11:46 PM
 */

#ifndef HTTPS_API_H
#define	HTTPS_API_H

#ifdef	__cplusplus
extern "C" {
#endif
    
#include "util/uthash.h"
#include "util/jsmn.h"
    
#define HTTP_OK                  200
#define HTTP_CREATED             201
#define HTTP_ACCEPTED            202
#define HTTP_NO_CONTENT          204
#define HTTP_BAD_REQUEST         400
#define HTTP_UNAUTHORIZED        401
#define HTTP_NOT_FOUND           404
#define HTTP_UNPROCESSABLE       422
#define HTTP_SERVER_ERROR        500
#define HTTP_SERVICE_UNAVAILABLE 503

typedef struct _param_t {
    char *name;
    char *value;             // NULL if json_type is JSMN_ARRAY or JSMN_OBJECT
    int json_type;
    int array_size;          // always 0 if json_type != JSMN_ARRAY
    struct _param_t *nested; // a list if array, a hash if object, NULL otherwise
    UT_hash_handle hh;
} param_t;


typedef struct {
    char *name;
    char *value;
    UT_hash_handle hh;
} header_t;


typedef struct {
    // UT_hash of params
    param_t *params;

    // UT_hash of request headers
    header_t *request_headers;

    // UT_hash of response headers
    header_t *response_headers;

    /*
      If called, performs HTTP basic auth against the specified headers
      and then returns 0 for successful authentication or 1 for failure.
    */
    int (*authenticate)(header_t *headers);
} request_t;

/*
 * An endpoint must return an HTTP status code and may optionally place a
 * text in the `response_body`. If no text is provided, a generic response
 * body for the specified status code is used.
 */
typedef int (*api_endpoint)(request_t *request);

#ifdef	__cplusplus
}
#endif

#endif	/* HTTPS_API_H */

