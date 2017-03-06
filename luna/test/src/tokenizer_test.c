#include "config.h"
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "services.h"
#include "util/detokenize_template.h"
#include "util/encryption_helpers.h"

#define ASSERT(a, b...) if (a) LINFO("PASS: " #a); else { LERROR("FAIL: " #a ": " b); err++; }

int main(int argc, char **argv) {
  int err = 0;
  size_t len;

  init_logger_service(LOG_LEVEL_INSEC);
  if (init_tokenizer_service()) return 1;
  if (init_encryption())        return 1;

  char *out = NULL;
  size_t size = 0;
  char template[2048];

  token_id t1 = create_token("secret", strlen("secret") + 1, "shhh");
  token_id t2 = create_token("moar",   strlen("moar") + 1,   "shhh");
  token_id t3 = create_token("binary\0data", strlen("binary data"), "shh");

  ASSERT(t1 > 0);
  ASSERT(t2 > 0);
  ASSERT(t1 != t2, "%u == %u", t1, t2);

  token_data(t2, (void **) &out, &size);
  ASSERT(out, "got: %s", out);
  ASSERT(size == 5, "got: %d", (int) size);
  ASSERT(!strcmp(out, "moar"), "got: %s", out);
  free(out);

  token_representation(t2, &out);
  ASSERT(!strcmp(out, "shhh"), "got: %s", out);
  free(out);

  len = sprintf(template, "%s%llu%s",
                TOKEN_PREFIX, (long long unsigned) t1, TOKEN_SUFFIX);
  out = detokenize_template(template, &len);
  ASSERT(!memcmp(out, "secret", 6 * sizeof(char)));
  free(out);

  len = sprintf(template, "%s%llu%s",
                TOKEN_PREFIX, (long long unsigned) t3, TOKEN_SUFFIX);
  out = detokenize_template(template, &len);
  ASSERT(len == 11);
  ASSERT(!memcmp(out, "binary\0data", len * sizeof(char)));
  free(out);

  len = sprintf(template, "hide %s%llu%s %s%llu%sz",
                TOKEN_PREFIX, (long long unsigned) t2, TOKEN_SUFFIX,
                TOKEN_PREFIX, (long long unsigned) t1, TOKEN_SUFFIX);
  out = detokenize_template(template, &len);
  ASSERT(!strcmp(out, "hide moar secretz"), "got: %s", out);
  free(out);

  ASSERT(free_token(t2) == 0);
  len = sprintf(template, "hide %s%llu%s %s%llu%sz",
                TOKEN_PREFIX, (long long unsigned) t2, TOKEN_SUFFIX,
                TOKEN_PREFIX, (long long unsigned) t1, TOKEN_SUFFIX);
  out = detokenize_template(template, &len);
  ASSERT(!strstr(out, "hide " TOKEN_PREFIX), "got: %s", out);
  ASSERT(!strstr(out, TOKEN_SUFFIX " secretz"), "got: %s", out);
  free(out);

  ASSERT(nuke_tokens() == 0);
  len = sprintf(template, "hide %s%llu%s %s%llu%sz",
                TOKEN_PREFIX, (long long unsigned) t2, TOKEN_SUFFIX,
                TOKEN_PREFIX, (long long unsigned) t1, TOKEN_SUFFIX);
  out = detokenize_template(template, &len);
  ASSERT(!strstr(out, "hide " TOKEN_PREFIX), "got: %s", out);
  ASSERT(!strstr(out, TOKEN_SUFFIX " " TOKEN_PREFIX), "got: %s", out);
  free(out);

  shutdown_tokenizer_service();
  shutdown_logger_service();

  return err;
}
