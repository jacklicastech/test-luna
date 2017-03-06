#define _GNU_SOURCE
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#include "util/files.h"
#include "services/logger.h"

int test_expand_path(void) {
  char *f;
  char *cwd = get_current_dir_name();
  char buf[PATH_MAX];

  int err = 0;
  setenv("HOME", "/home/test", 1);
  #define TEST_EXPAND(a, b, c)                                               \
                            LINFO("now testing: expand_path(%s, %s) == %s",  \
                                  #a, #b, c);                                \
                            if (strcmp(f = expand_path(a, b), c))            \
                              LERROR("FAIL %d: got %s", ++err, f);           \
                            free(f);

  #define TEST_EXPAND_RELATIVE(a, b, c)                                      \
                            sprintf(buf, "%s/%s", cwd, c);                   \
                            TEST_EXPAND(a, b, buf);

  TEST_EXPAND("~",           NULL, "/home/test");
  TEST_EXPAND("~oracle",     NULL, "/home/oracle");
  TEST_EXPAND("~oracle/bin", NULL, "/home/oracle/bin");

  // if home is specified, dir doesn't matter
  TEST_EXPAND("~",           "a", "/home/test");
  TEST_EXPAND("~oracle",     "a", "/home/oracle");
  TEST_EXPAND("~oracle/bin", "a", "/home/oracle/bin");

  TEST_EXPAND("~oracle/.",                NULL, "/home/oracle");
  TEST_EXPAND("~oracle/bin/../src",       NULL, "/home/oracle/src");
  TEST_EXPAND("~oracle/bin/../../../",    NULL, "/");
  TEST_EXPAND("~oracle/bin/../../../../", NULL, "/");
  TEST_EXPAND("~oracle/bin/../../../..",  NULL, "/");

  TEST_EXPAND_RELATIVE("t",               NULL, "t");

  TEST_EXPAND("t",     "/", "/t");
  TEST_EXPAND("t///t", "/", "/t/t");

  TEST_EXPAND_RELATIVE("filename", "./t/..", "filename");
  TEST_EXPAND("settings.db", "/mnt/hgfs/HostDocs/luna/test", "/mnt/hgfs/HostDocs/luna/test/settings.db")

  free(cwd);
  return err;
}

int main() {
  int err;

  init_logger_service(LOG_LEVEL_DEBUG);

  err = test_expand_path();

  shutdown_logger_service();
  return err;
}
