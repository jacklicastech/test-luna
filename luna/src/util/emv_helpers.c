#include "services.h"
#include "util/string_helpers.h"

/*
 * Expects a hexadecimal string in the format described at
 * http://www.emvlab.org/emvtags/show/t57/ , and replaces 'd' with '='
 * and removes padding characters, if any, so that the result is equal to
 * the MSR track 2 format. Returns a token ID pointing to the data, not the
 * data itself.
 */
token_id parse_emv_track2_equiv(const char *hex) {
  char mask[] = { '4', '*', '*', '*', '*', '*', '*', '*',
                  '*', '*', '*', '*', '1', '1', '1', '1', '\0' };
  char *x = NULL;
  if (!strncmp(hex, "3b", 2) || !strncmp(hex, "3B", 2)) {
    // this is a hex-encoded start sentinel, only present for certain schemes.
    // If not present, the hex data as-is represents track 2 data. If present,
    // the hex data needs to be decoded into binary first, and then the start
    // sentinel, end sentinel and LRC need to be skipped.
    size_t len = strlen(hex);
    x = hex2bytes(hex, &len);
    char *y = x + 1;
    size_t i;
    for (i = 0; i < len - 1; i++) {
      char ch = *(y + i);
      if (ch == '\0') break;
      if (ch == '?') { *(y + i) = '\0'; break; }
    }
    y = strdup(y);
    free(x);
    x = y;
  } else
    x = strdup(hex);
  int i;
  for (i = 0; i < strlen(x); i++) {
    if (*(x+i) == 'd' || *(x+i) == 'D') {
      *(x+i) = '=';
      // populate mask if we have enough digits to do so
      if (i > 5) {
        mask[ 0] = *x;
        mask[12] = *(x + i - 4);
        mask[13] = *(x + i - 3);
        mask[14] = *(x + i - 2);
        mask[15] = *(x + i - 1);
      }
    }
  }
  if (*(x + strlen(x) - 1) == 'F' || *(x + strlen(x) - 1) == 'f')
    *(x + strlen(x) - 1) = '\0';

  LINSEC("util: emv: track2-equivalent data (hex): %s", x);
  token_id track2_data_token = create_token(x, strlen(x), mask);
  free(x);
  return track2_data_token;
}
