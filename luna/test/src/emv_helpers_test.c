#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "services.h"
#include "util/emv_helpers.h"
#include "util/detokenize_template.h"
#include "util/encryption_helpers.h"

int assert_parsed_as(const char *src, const char *dst) {
  char buf[1024];
  token_id tok = parse_emv_track2_equiv(src);
  sprintf(buf, TOKEN_PREFIX "%u" TOKEN_SUFFIX, (unsigned) tok);
  size_t len = strlen(buf);
  char *track2 = detokenize_template(buf, &len);
  int result = (int) strcmp(track2, dst);
  if (result) printf("FAIL: %s != %s\n", track2, dst);
  free(track2);
  return result;
}

int main(int argc, char **argv) {
  int err = 0;
  if ((err = init_logger_service(LOG_LEVEL_INSEC))) goto shutdown;
  if ((err = init_settings_service()))              goto shutdown;
  if ((err = init_encryption()))                    goto shutdown;
  if ((err = init_tokenizer_service()))             goto shutdown;

  // visa/amex
  err = err | assert_parsed_as("3b343736313733393030313031303031303d3139313232303131313433383832353f09",
                               "4761739001010010=191220111438825");
  err = err | assert_parsed_as("3B343736313733393030313031303031303D3139313232303131313433383832353F09",
                               "4761739001010010=191220111438825");

  // mastercard
  err = err | assert_parsed_as("5413330089601042d25122210123409172f",
                               "5413330089601042=25122210123409172");

  err = err | assert_parsed_as("5413330089601042D25122210123409172F",
                               "5413330089601042=25122210123409172");

shutdown:
  shutdown_tokenizer_service();
  shutdown_settings_service();
  shutdown_logger_service();

  return err;
}
