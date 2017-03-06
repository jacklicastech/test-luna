/* 
 * File:   api_request.h
 * Author: Colin
 *
 * Created on February 12, 2016, 1:07 AM
 */

#ifndef API_REQUEST_H
#define	API_REQUEST_H

#include "rest_api.h"

#define MAX_HTTP_VERB_LENGTH  7
#define MAX_HTTP_PATH_LENGTH  2048

#ifdef	__cplusplus
extern "C" {
#endif

char *dispatch_request(const char *verb, const char *path,
                       header_t **headers, const char *request_body);

unsigned int scan_http_path(const char *request,
                            unsigned int request_length,
                            char *verb,
                            char *path,
                            unsigned int max_path_len);

#ifdef	__cplusplus
}
#endif

#endif	/* API_REQUEST_H */

