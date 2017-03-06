#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include "services.h"
#include "util/detokenize_template.h"

char *parse_template(const char *data, size_t *len,
                     int (*fetch_data)(token_id id, void **data, size_t *size)) {
  const char *token_signal_start = TOKEN_PREFIX;
  const char *token_signal_end   = TOKEN_SUFFIX;
  const size_t token_signal_start_len = strlen(token_signal_start);
  const size_t token_signal_end_len   = strlen(token_signal_end);
  const size_t data_len = *len;
  char *result = calloc(1, (data_len + 1) * sizeof(char));
  size_t result_size = data_len;
  long offset = 0;
  size_t i, j;
  long long token;

  for (i = 0; i < data_len; i++) {
    if (!strncmp(data + i, token_signal_start, token_signal_start_len)) {
      int found = 0;
      for (j = i + token_signal_start_len; j < data_len; j++) {
        if (!strncmp(data + j, token_signal_end, token_signal_end_len)) {
          size_t token_len = j - (i + token_signal_start_len);
          char *token_str = calloc(token_len + 1, sizeof(char));
          sprintf(token_str, "%.*s", (int) token_len, data + (i + token_signal_start_len));
          token = atoll(token_str);
          free(token_str);
          token_str = NULL;
          i = j + strlen(token_signal_end) - 1;
          LDEBUG("detokenizer: parsed token: %lld", token);
          found = 1;
          break;
        }
      }

      if (found) {
        void *token_str = NULL;
        size_t token_size = 0;
        fetch_data(token, &token_str, &token_size);
        if (token_str == NULL) {
          LWARN("detokenizer: referenced token does not exist: %lld", token);
        } else {
          LINSEC("detokenizer: referenced token is %llu bytes", (long long unsigned) token_size);
          if (*((char *)token_str + token_size - 1) == '\0') {
            LTRACE("detokenizer: last byte is NUL");
            token_size--;
          }
          result_size += 1 + token_size;
          result = realloc(result, result_size * sizeof(char));
          memcpy(result + offset, token_str, token_size * sizeof(char));
          offset += token_size;
          *(result + offset) = '\0';
          free(token_str);
          token_str = NULL;
        }
      }
    } else {
      *(result + offset++) = *(data + i);
      *(result + offset) = '\0';
    }
  }

  *len = offset;
  return result;
}

int get_token_data(token_id id, void **data, size_t *size) {
  return token_data(id, data, size);
}

int get_token_human(token_id id, void **data, size_t *size) {
  int err = token_representation(id, (char **) data);
  if (!err) *size = strlen(*data);
  return err;
}

char *detokenize_template(const char *data, size_t *len) {
  return parse_template(data, len, get_token_data);
}

char *humanize_template(const char *data, size_t *len) {
  return parse_template(data, len, get_token_human);
}
