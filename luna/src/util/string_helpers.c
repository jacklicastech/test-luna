#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Trims white space from the given string, returning it. The string is
 * modified in-place.
 */
char *trim(char *str) {
  char *end;

  // Trim leading space
  while(isspace(*str)) str++;

  if(*str == 0)  // All spaces?
    return str;

  // Trim trailing space
  end = str + strlen(str) - 1;
  while(end > str && isspace(*end)) end--;

  // Write new null terminator
  *(end+1) = '\0';

  return str;
}

/*
 * Returns a NULL-terminated string containing the hex representation of the
 * bytes in input data. The string must be freed.
 */
char *bytes2hex(const char *data, size_t len) {
  char *str = calloc(1, (len * 2 + 1) * sizeof(char));
  size_t i;
  for (i = 0; i < len; i++) {
    sprintf(str + i * 2, "%02x", (unsigned char) data[i]);
  }
  return str;
}

/*
 * Converts a hex string into a binary-coded decimal representation of the
 * same value. `len` contains the length of the hex string on input and the
 * length of the BCD string on output.
 */
unsigned char *hex2bcd(const char *hex, size_t *len) {
  *len = (*len) / 2;
  size_t hex_len = (*len) * 2;
  unsigned char *bcd = calloc(1, sizeof(char) * *len);
  unsigned char val, tmp;
  char ch;
  int n = 0, p = 0, i;
  while (n < hex_len) {
    val = 0x00;
    for (i = 0; i < 2; i++) {
      ch = hex[n++];
      if (isdigit(ch)) tmp = ch - '0';
      else tmp = ch - 'A' + 10;
      val |= tmp;
      if (i == 0) val <<= 4;
    }
    bcd[p++] = val;
  }

  return bcd;
}

/*
 * Returns a string of bytes by decoding the hex representation. `len` is the
 * length of the hex representation on input, and the length of the string of
 * bytes on output. The string of bytes must be freed.
 *
 * If the input is not valid hex, `len` is not modified and NULL is returned.
 */
char *hex2bytes(const char *hex, size_t *len) {
  size_t real_len = *len;
  // don't consider the null terminator if present
  if (real_len > 0 && *(hex + real_len - 1) == '\0') real_len--;
  size_t blob_len = real_len / 2 + (real_len % 2);
  unsigned char *blob = calloc(1, sizeof(unsigned char) * blob_len);
  unsigned char *result = blob;
  if (real_len % 2 == 1) {
    // odd number of characters, read only 1 digit for first byte, e.g.
    // 0x1 == 0x01
    if (sscanf(hex, "%1hhx", blob) < 1)
      goto invalid_hex;
    hex++;
    blob++;
  }
  int i;
  for (i = 0; i < blob_len; i++)
    if (sscanf(hex + i * 2, "%2hhx", blob + i) < 1)
      goto invalid_hex;
  *len = blob_len;
  return (char *) result;
invalid_hex:
  free(result);
  return NULL;
}
