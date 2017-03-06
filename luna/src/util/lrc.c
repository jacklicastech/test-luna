#include <stdio.h>
#include "util/lrc.h"

char lrc(const char *str, size_t len) {
  char lrc = '\0';
  if (len > 0) lrc = *str;
  size_t i;
  for (i = 1; i < len; i++)
    lrc = lrc ^ *(str + i);
  return lrc;
}
