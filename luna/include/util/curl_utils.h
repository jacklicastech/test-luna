/* 
 * File:   curl_utils.h
 * Author: Colin
 *
 * Created on January 24, 2016, 12:14 AM
 */

#ifndef CURL_UTILS_H
#define	CURL_UTILS_H

#include <curl/curl.h>

struct MemoryStruct {
  char *memory;
  size_t size;
};

/*
 * Accumulates payload data into the string object. Useful for responses
 * known to be relatively small; bad idea for larger payloads like file
 * downloads. Assumes payload data will be a NULL-terminated string, and
 * ensures the result is a NULL-terminated string. Not useful for arbitrary
 * binary data.
 */
size_t curl_cb_accum_cstr(void *ptr, size_t size, size_t nmemb, char **str);

/*
 * Accumulates payload data into the memory object. Useful for responses
 * known to be relatively small; bad idea for larger payloads like file
 * downloads.
 */
size_t curl_cb_accum_mem(void *ptr, size_t size, size_t nmemb, char **str);

/*
 * Writes payload data to the specified open file handle.
 */
size_t curl_cb_write_file(void *ptr, size_t size, size_t nmemb, FILE *handle);

extern char cacerts_bundle[PATH_MAX];

#ifdef __cplusplus
}
#endif

#endif	/* CURL_UTILS_H */

