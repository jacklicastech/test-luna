#include <stdlib.h>
#include <memory.h>
#include <errno.h>
#include "util/curl_utils.h"
#include "services.h"

size_t curl_cb_accum_cstr(void *ptr, size_t size, size_t nmemb, char **str) {
  char *buf = (char *) calloc(size * nmemb + 1, sizeof(char));
  memcpy(buf, ptr, size * nmemb);
  int nul = *str == NULL;
  size_t total_len = (nul ? 0 : strlen(*str)) + size * nmemb + 1;
  *str = (char *) realloc(*str, total_len * sizeof(char));
  if (nul) **str = '\0';
  strcat(*str, buf);
  free(buf);
  return size * nmemb;
}

size_t curl_cb_accum_mem(void *contents, size_t size, size_t nmemb, char **userp) {
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory! */ 
    LWARN("curl_cb_accum_str: not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
 
  return realsize;
}

size_t curl_cb_write_file(void *ptr, size_t size, size_t nmemb, FILE *handle) {
  size_t written = 0, to_write = size * nmemb;
  while (written < to_write) {
    size_t res = fwrite((char *)ptr + written, 1, size * nmemb - written, handle);
    if (res != size * nmemb - written) {
      switch(errno) {
        case 0: break;
        case EAGAIN: break;
        default:
          LWARN("auto-update: download: error while writing file: %s", strerror(errno));
          return written + res;
      }
    }
    if (res > 0)
      written += res;
  }
  return written;
}
