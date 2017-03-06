#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "util/string_helpers.h"

void test_hex2bcd(void) {
  const char *hex = "001122";
  size_t len = strlen(hex) + 1;
  unsigned char *blob = hex2bcd(hex, &len);
  assert(len == 3);
  assert(*(blob    ) == '\x00');
  assert(*(blob + 1) == '\x11');
  assert(*(blob + 2) == '\x22');
  free(blob);
}

void test_hex2bytes(void) {
  const char *hex = "001122";
  size_t len = strlen(hex) + 1;
  char *blob = hex2bytes(hex, &len);
  assert(len == 3);
  assert(*(blob    ) == '\x00');
  assert(*(blob + 1) == '\x11');
  assert(*(blob + 2) == '\x22');
  free(blob);

  // test empty strings of varying forms
  hex = "";
  len = 1; // null terminator
  blob = hex2bytes(hex, &len);
  assert(len == 0);
  free(blob);

  hex = "";
  len = 0; // pretend there's no null
  blob = hex2bytes(hex, &len);
  assert(len == 0);
  free(blob);

  // what if hex is immediately adjacent to a null? (yes, this was an issue.)
  char *a = calloc(1, sizeof(char));
  *a = '\0';
  len = 0;
  blob = hex2bytes(a + 1, &len);
  assert(len == 0);
  free(blob);
  free(a);

  len = 6;
  blob = hex2bytes("HOSTOK", &len); // not valid hex
  assert(blob == NULL); // return NULL
  assert(len == 6);     // leave len unchanged
}

int main() {
  test_hex2bytes();
  test_hex2bcd();
  return 0;
}
