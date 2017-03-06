#define _GNU_SOURCE // not needed for debian, but needed for MP200/V3000
#include <string.h>

#include "services.h"
#include "util/headers_parser.h"
#include "util/string_helpers.h"
#include "util/uthash.h"

int parse_headers(header_t **headers, const char *headers_and_body, int len) {
#define STATE_HAVE_NOTHING 0
#define STATE_HAVE_NAME 1
    int i, j;
    int name_start = 0, value_start = 0;
    int state = STATE_HAVE_NOTHING;
    char *name = NULL, *value = NULL;
    header_t *header = NULL, *tmp = NULL;
    int newline_count = 0;

    for (i = 0; newline_count < 2 && i < len; i++) {
        if (state == STATE_HAVE_NOTHING) { // searching for header name
            if (*(headers_and_body + i) == ':') {
                name = (char *) calloc((i - name_start) + 4, sizeof(char));
                snprintf(name, i - name_start + 1, "%s", headers_and_body + name_start);
                state = STATE_HAVE_NAME;
                value_start = i + 1;
                newline_count = 0;
            } else if (*(headers_and_body + i) == '\n') {
                name_start = i + 1;
                value_start = 0;
                newline_count += 1;
            } else if (*(headers_and_body + i) != '\r') {
                newline_count = 0;
            }
        } else {
            if (*(headers_and_body + i) == '\n') {
                for (j = 0; name[j]; j++) name[j] = tolower(name[j]);
                value = strndup(headers_and_body + value_start, i - value_start + 2);
                value[i - value_start] = '\0';

                header = (header_t *) calloc(1, sizeof(header_t));
                header->name = name;
                header->value = (char *) calloc(strlen(trim(value)) + 4, sizeof(char));
                sprintf(header->value, "%s", trim(value));
                free(value);

                HASH_ADD_KEYPTR(hh, *headers, name, strlen(name), header);
                state = STATE_HAVE_NOTHING;
                value_start = 0;
                name_start = i + 1;
                name = NULL;
                newline_count += 1;
            } else if (*(headers_and_body + i) != '\r') {
                newline_count = 0;
            }
        }
    }

    if (state == STATE_HAVE_NAME) {
        for (j = 0; name[j]; j++) name[j] = tolower(name[j]);
        value = strndup(headers_and_body + value_start, i - value_start);
        header = (header_t *) calloc(1, sizeof(header_t));
        header->name = name;
        header->value = strdup(trim(value));
        free(value);
        HASH_ADD_KEYPTR(hh, *headers, name, strlen(name), header);
        name = NULL;
    }

    LDEBUG("Request Headers:");
    HASH_ITER(hh, *headers, header, tmp) {
        LDEBUG("  %s: %s", header->name, header->value);
    }

    if (name) free(name);

    return i;
#undef STATE_HAVE_NOTHING
#undef STATE_HAVE_NAME
}

void free_headers(header_t **headers) {
  header_t *header, *tmp;

  HASH_ITER(hh, *headers, header, tmp) {
    HASH_DEL(*headers, header);
    free(header->name);
    free(header->value);
    free(header);
  }
}
