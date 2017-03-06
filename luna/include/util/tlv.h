#ifndef UTIL_TLV_H
#define UTIL_TLV_H

#include "util/uthash.h"

typedef struct {
  size_t tag_length;
  char *tag;

  size_t value_length;
  unsigned char *value;

  UT_hash_handle hh;
} tlv_t;

/*
 * Parses the given data in TLV format (NOT a hex-escaped string) into a hash
 * which can be indexed using EMV tags.
 *
 * The function can fail if the data is malformed or not parseable. If so,
 * the resultant hash will contain any tags which were successfully processed.
 */
int tlv_parse(unsigned char *data, size_t data_len, tlv_t **out);

tlv_t *tlv_new(void);
void tlv_free(tlv_t **tlv);
void tlv_freeall(tlv_t **head);
size_t tlv_encode(tlv_t **head, char **out);
void tlv_sanitize(tlv_t **head);

void dump_blob_as_hex(unsigned char *data, size_t len, const char *log_prefix);

#endif // UTIL_TLV_H
