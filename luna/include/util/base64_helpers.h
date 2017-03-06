#ifndef UTIL_BASE64_HELPERS_H
#define UTIL_BASE64_HELPERS_H

char *base64_encode(const char *data, size_t size);
void base64_decode(const char *b64, char **out, size_t *out_size);

#endif // UTIL_BASE64_HELPERS_H
