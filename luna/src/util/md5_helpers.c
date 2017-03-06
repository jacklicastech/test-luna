#include "config.h"
#include <stdlib.h>
#include "services.h"
#include "util/md5_helpers.h"

int md5_matches(const char *path, const char *expected_md5) {
  MD5_CTX ctx;
  char buf[1024];
  unsigned char digest[MD5_DIGEST_LENGTH];
  char actual_md5[MD5_DIGEST_LENGTH * 2];
  int i;
  FILE *file = NULL;
  int error = 0;

  file = fopen(path, "rb");
  if (!file) {
    LERROR("md5: couldn't open file %s for reading", path);
    return 0;
  }

  MD5_Init(&ctx);
  while (!error && !feof(file)) {
    memset(buf, 0, sizeof(buf));
    size_t bytes_read = fread(buf, 1, sizeof(buf) - 1, file);
    if (bytes_read >= 0) {
      buf[bytes_read] = '\0';
      MD5_Update(&ctx, buf, bytes_read);
    }
    if (!feof(file) && bytes_read != sizeof(buf) - 1) {
      switch(errno) {
        case EAGAIN: // retry
          break;
        default:
          LERROR("md5: error '%s' while reading file %s", strerror(errno), path);
          error = 1;
          break;
      }
    }
  }
  MD5_Final(digest, &ctx);
  fclose(file);
  if (error) return 0;

  for (i = 0; i < MD5_DIGEST_LENGTH; i++)
    sprintf(((char *)actual_md5) + i*2, "%02x", (unsigned int) digest[i]);

  if (strncmp(actual_md5, expected_md5, MD5_DIGEST_LENGTH * 2)) {
    LERROR("md5: mismatch for file %s (%s != %s)", path, actual_md5, expected_md5);
    return 0;
  }

  return 1;
}
