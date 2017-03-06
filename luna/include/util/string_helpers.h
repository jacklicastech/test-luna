#ifndef UTIL_STRING_HELPERS_H
#define UTIL_STRING_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

char *trim(char *str);
char *bytes2hex(const char *data, size_t len);
char *hex2bytes(const char *hex, size_t *len);
unsigned char *hex2bcd(const char *hex, size_t *len);

#ifdef __cplusplus
}
#endif

#endif // UTIL_STRING_HELPERS_H
