#include <assert.h>

#include "services/logger.h"
#include "services/settings.h"
#include "util/tlv.h"
#include "util/string_helpers.h"

void test_decode() {
  const char *hex = "910100BF0101019F180102";
  size_t len = strlen(hex);
  char *bytes = hex2bytes(hex, &len);

  tlv_t *head = NULL;
  tlv_t *entry = NULL;
  assert(!tlv_parse((unsigned char *) bytes, len, &head));
  free(bytes);

  HASH_FIND_STR(head, "\xBF\x01", entry);
  assert(!!entry);
  assert(*entry->value == '\x01');

  entry = NULL;
  HASH_FIND_STR(head, "\x9F\x18", entry);
  assert(!!entry);
  assert(*entry->value == '\x02');

  tlv_freeall(&head);
}

void test_encode() {
  const char *hex = "910100bf0101019f180102";
  size_t len = strlen(hex);
  char *bytes = hex2bytes(hex, &len);

  tlv_t *head = NULL;
  assert(!tlv_parse((unsigned char *) bytes, len, &head));
  free(bytes);

  len = tlv_encode(&head, &bytes);
  char *hex2 = bytes2hex(bytes, len);
  assert(len == strlen(hex) / 2);
  assert(!memcmp(hex, hex2, len));

  free(hex2);
  free(bytes);
  tlv_freeall(&head);
}

void test_sanitize_duplicate() { // maybe caused segfault?
  const char *hex = "5A01005A0100";
  size_t len = strlen(hex);
  char *bytes = hex2bytes(hex, &len);

  tlv_t *head = NULL;
  assert(!tlv_parse((unsigned char *) bytes, len, &head));
  free(bytes);

  tlv_sanitize(&head);
  tlv_freeall(&head);
}

int main(int argc, char **argv) {
  init_logger_service(LOG_LEVEL_INSEC);
  test_decode();
  test_encode();
  test_sanitize_duplicate();

  return 0;
}