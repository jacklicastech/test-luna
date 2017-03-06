#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <assert.h>

#include "util/base64_helpers.h"

char *base64_encode(const char *data, size_t size) {
  BIO *bio, *b64;
  BUF_MEM *bufferPtr;
  char *out;

  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new(BIO_s_mem());
  bio = BIO_push(b64, bio);

  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Ignore newlines - write everything in one line
  BIO_write(bio, data, size);
  BIO_flush(bio);
  BIO_get_mem_ptr(bio, &bufferPtr);
  BIO_set_close(bio, BIO_NOCLOSE);
  BIO_free_all(bio);

  out = strndup(bufferPtr->data, bufferPtr->length);
  free(bufferPtr->data);
  free(bufferPtr);

  return out;
}

size_t base64_decode_size(const char *b64input) {
  size_t len = strlen(b64input),
    padding = 0;

  if (b64input[len-1] == '=' && b64input[len-2] == '=') //last two chars are =
    padding = 2;
  else if (b64input[len-1] == '=')
    padding = 1;

  return (len*3)/4 - padding;
}

void base64_decode(const char *b64message, char **buffer, size_t *length) {
  BIO *bio, *b64;
  char *clone = strdup(b64message);

  int decodeLen = base64_decode_size(clone);
  *buffer = (char *) malloc(decodeLen + 1);
  (*buffer)[decodeLen] = '\0';

  bio = BIO_new_mem_buf(clone, -1);
  b64 = BIO_new(BIO_f_base64());
  bio = BIO_push(b64, bio);

  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Do not use newlines to flush buffer
  *length = BIO_read(bio, *buffer, strlen(clone));
  assert(*length == decodeLen); //length should equal decodeLen, else something went horribly wrong
  BIO_free_all(bio);
  free(clone);
}
