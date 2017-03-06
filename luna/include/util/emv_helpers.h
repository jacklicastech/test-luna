#ifndef UTIL_EMV_HELPERS_H
#define UTIL_EMV_HELPERS_H

  #include <stddef.h>
  #include "services/tokenizer.h"

  /*
   * Expects a hexadecimal string in the format described at
   * http://www.emvlab.org/emvtags/show/t57/ , and replaces 'd' with '='
   * and removes padding characters, if any, so that the result is equal to
   * the MSR track 2 format. Returns a token ID pointing to the data, not the
   * data itself.
   */
  token_id parse_emv_track2_equiv(const char *hex);

#endif // UTIL_EMV_HELPERS_H
