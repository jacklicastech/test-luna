#define READ_TAG_NAME 1
#define READ_TAG_NAME_LONG_FORM 2
#define READ_TAG_LENGTH_UNKNOWN 3
#define READ_TAG_LENGTH_LE255 4
#define READ_TAG_LENGTH_LE65535 5
#define READ_TAG_VALUE 6

#include <assert.h>
#include <stdio.h>
#include "services/logger.h"
#include "util/string_helpers.h"
#include "util/tlv.h"

// Although we only really expect the PAN and track 2 equivalent data to be
// considered sensitive, it's always safer to whitelist than to blacklist.
const char * WHITELISTED_TAGS[] = {
  "\x9f\x08", // app version number
  "\x9f\x1e", // IFD serial number
  "\x9f\x39", // POS entry mode
  "\x9f\x02", // authorized amount, numeric
  "\x9f\x03", // other amount, numeric
  "\x9f\x26", // application cryptogram
  "\x5f\x24", // application expiration date
  "\x82",     // application interchange profile
  "\x50",     // application label
  "\x5f\x34", // PAN sequence number
  "\x9f\x12", // application preferred name
  "\x9f\x36", // application transaction counter
  "\x9f\x09", // application version number
  "\x9f\x27", // cryptogram information data
  "\x9f\x34", // CVM results
  "\x84",     // dedicated file name
  "\x9f\x10", // issuer application data
  "\x9f\x11", // issuer code table index
  "\x9f\x33", // terminal capabilities
  "\x9f\x1a", // terminal country code
  "\x9f\x35", // terminal type
  "\x95",     // terminal verification results
  "\x5f\x2a", // transaction currency code
  "\x9a",     // transaction date
  "\x9c",     // transaction type
  "\x9f\x37", // unpredictable number
  "\x9f\x6e", // third party data (omaha: device type)
  "\x9f\x6d", // application version number/kernel caps/vlp reset threshold
  "\x4f",     // application id (card)
  "\x9f\x06", // application id (terminal)
  "\x9f\x07", // application usage control
  "\xdf\xc6", // TAC denial (contact)
  "\xdf\xc7", // TAC online (contact)
  "\xdf\xc8", // TAC default (contact)
  "\xdf\x81\x20", // TAC default (contactless)
  "\xdf\x81\x21", // TAC denial (contactless)
  "\xdf\x81\x22", // TAC online (contactless)
  "\x9f\x41", // transaction sequence counter
  NULL
};

void tlv_sanitize(tlv_t **head) {
  tlv_t *tmp = NULL, *tlv = NULL;
  int i, removed = 0;

  HASH_ITER(hh, *head, tlv, tmp) {
    int found = 0;
    for (i = 0; WHITELISTED_TAGS[i] != NULL; i++) {
      if (!strcmp(WHITELISTED_TAGS[i], tlv->tag)) {
        found = 1;
        break;
      }
    }
    if (!found) {
      removed++;
      HASH_DEL(*head, tlv);
      char *hex = bytes2hex(tlv->tag, tlv->tag_length);
      LTRACE("tlv-sanitize: removed potentially sensitive tag %s", hex);
      free(hex);
      tlv_free(&tlv);
    }
  }

  LDEBUG("tlv-sanitize: removed %d sensitive tags", removed);
}

void tlv_freeall(tlv_t **head) {
  tlv_t *tmp, *tlv;
  HASH_ITER(hh, *head, tlv, tmp) {
    HASH_DEL(*head, tlv);
    tlv_free(&tlv);
  }
}

size_t tlv_encode(tlv_t **head, char **out) {
  tlv_t *tmp = NULL, *tlv = NULL;
  size_t total_size = 0;

  *out = NULL;
  size_t offset = 0;
  HASH_ITER(hh, *head, tlv, tmp) {
    size_t tlen = tlv->tag_length;
    size_t llen = tlv->value_length > 255 ? 3 :
                  tlv->value_length > 127 ? 2 : 1;
    size_t vlen = tlv->value_length;
    total_size += tlen + llen + vlen;
    *out = realloc(*out, total_size);

    dump_blob_as_hex((unsigned char *) tlv->tag, tlen, "tlv-encode: tag name");
    LTRACE("tlv-encode:   length: %d (%d)", tlv->value_length, llen);
    dump_blob_as_hex((unsigned char *) tlv->value, vlen, "tlv-encode:    value");
    LTRACE("tlv-encode: total size is now %d bytes", (int) total_size);
    LTRACE("tlv-encode: offset is currently %d", (int) offset);

    char *ptr = *out + offset;

    // copy tag name
    memcpy(ptr, tlv->tag, tlen);
    ptr += tlv->tag_length;

    // write tag value length, there's some logic to this
    if (tlv->value_length > 255) {
      *ptr = 0x80 | 0x02;
      ptr++;
      *ptr = tlv->value_length >> 8;
      ptr++;
      *ptr = tlv->value_length % 0xFF;
      ptr++;
    } else if (tlv->value_length > 127) {
      *ptr = 0x80 | 0x01;
      ptr++;
      *ptr = tlv->value_length;
      ptr++;
    } else {
      *ptr = tlv->value_length;
      ptr++;
    }

    // write tag value
    memcpy(ptr, tlv->value, vlen);
    ptr += tlv->value_length;

    offset = total_size;
  }

  dump_blob_as_hex((unsigned char *) *out, total_size, "tlv-encode: result");
  return total_size;
}

void dump_blob_as_hex(unsigned char *data, size_t len, const char *log_prefix) {
  size_t i, j = 0;
  char line[24 * 3 + 1];
  memset(line, 0, sizeof(line));
  for (i = 0; i < len; i++) {
    sprintf(line + strlen(line), "%02x ", data[i]);
    j++;
    if (j == 24) {
      LINSEC("%s: %s", log_prefix, line);
      memset(line, 0, sizeof(line));
      j = 0;
    }
  }
  if (strlen(line) > 0) LINSEC("%s: %s", log_prefix, line);
}

void tlv_free(tlv_t **tlv) {
  if (*tlv) {
    if ((*tlv)->value) free((*tlv)->value);
    if ((*tlv)->tag)   free((*tlv)->tag);
    free(*tlv);
    *tlv = NULL;
  }
}

tlv_t *tlv_new(void) {
  tlv_t *tlv = calloc(1, sizeof(tlv_t));
  return tlv;
}

int tlv_parse(unsigned char *data, size_t data_len, tlv_t **head) {
  #define FAIL(msg) LERROR("tlv-decode: " msg); result = 1; goto cleanup;

  int state = READ_TAG_NAME;
  tlv_t *current_tag = tlv_new(), *tmp = NULL;
  int i, j = 0;
  int result = 0;

  for (i = 0; i < data_len; i++) {
    unsigned char ch = *(data + i);
    switch(state) {
      case READ_TAG_NAME:
        current_tag->tag = realloc(current_tag->tag, current_tag->tag_length + 2);
        current_tag->tag[current_tag->tag_length++] = ch;
        current_tag->tag[current_tag->tag_length] = '\0';
        // if the last 5 bits are 1 then there is at least another byte in the
        // tag name
        if ((ch & 0x1f) == 0x1f) {
          LTRACE("tlv-decode: reading long-form tag name");
          state = READ_TAG_NAME_LONG_FORM;
        } else {
          state = READ_TAG_LENGTH_UNKNOWN;
          char *hex = bytes2hex((char *) current_tag->tag, current_tag->tag_length);
          LTRACE("tlv-decode: parsed short name: %s", hex);
          free(hex);
        }
        break;
      case READ_TAG_NAME_LONG_FORM:
        current_tag->tag = realloc(current_tag->tag, current_tag->tag_length + 2);
        current_tag->tag[current_tag->tag_length++] = ch;
        current_tag->tag[current_tag->tag_length] = '\0';
        // if the 8th bit is 1 then there is at least another byte in the tag
        // name
        if ((ch & 0x80) == 0x80) {
          LTRACE("tlv-decode: long-form tag name contains another byte");
        } else {
          char *hex = bytes2hex((char *) current_tag->tag, current_tag->tag_length);
          LTRACE("tlv-decode: parsed long name: %s", hex);
          free(hex);
          state = READ_TAG_LENGTH_UNKNOWN;
        }
        break;
      case READ_TAG_LENGTH_UNKNOWN:
        if (ch <= 127) {
          current_tag->value_length = ch;
          LTRACE("tlv-decode: tag length: %d", ch);
          state = READ_TAG_VALUE;
          j = current_tag->value_length;
          current_tag->value = calloc(1, j * sizeof(unsigned char));
        } else if (ch & 1) {
          state = READ_TAG_LENGTH_LE255;
          LTRACE("tlv-decode: tag length: 127 < len <= 255");
        } else if (ch & 2) {
          state = READ_TAG_LENGTH_LE65535;
          j = 2;
          LTRACE("tlv-decode: tag length: 255 < len <= 65535");
        } else {
          FAIL("failed to read length of tag");
        }
        break;
      case READ_TAG_LENGTH_LE255:
        LTRACE("tlv-decode: tag length: %d", ch);
        current_tag->value_length = ch;
        state = READ_TAG_VALUE;
        j = current_tag->value_length;
        current_tag->value = calloc(1, sizeof(j) * sizeof(unsigned char));
        break;
      case READ_TAG_LENGTH_LE65535:
        current_tag->value_length = (current_tag->value_length << 8) + ch;
        j--;
        assert(j >= 0);
        if (j == 0) {
          LTRACE("tlv-decode: tag length: %d", current_tag->value_length);
          state = READ_TAG_VALUE;
          j = current_tag->value_length;
          current_tag->value = calloc(1, sizeof(j) * sizeof(unsigned char));
        }
        break;
      case READ_TAG_VALUE:
        *(current_tag->value + current_tag->value_length - j) = ch;
        j--;
        assert(j >= 0);
        if (j == 0) {
          LTRACE("tlv-decode: tag value has been read");
          state = READ_TAG_NAME;
          dump_blob_as_hex((unsigned char *) current_tag->tag,   current_tag->tag_length,   "tlv-decode: tag name");
          LINSEC("tlv-decode: tag length: %d", current_tag->value_length);
          dump_blob_as_hex(current_tag->value, current_tag->value_length, "tlv-decode: tag value");
          HASH_FIND_STR(*head, current_tag->tag, tmp);
          if (tmp) {
            LWARN("tlv-decode: duplicate tag found in input, only the last will be kept");
            HASH_DEL(*head, tmp);
            tlv_free(&tmp);
          }
          HASH_ADD_KEYPTR(hh, *head, current_tag->tag, current_tag->tag_length, current_tag);
          current_tag = tlv_new();
        }
        break;
      default:
        LERROR("tlv-decode: entered an unknown state: %d", state);
        result = 1;
        goto cleanup;
    }
  }

  if (state != READ_TAG_NAME) {
    LWARN("tlv-decode: finished processing in an incomplete state (%d)", state);
  }

cleanup:
  tlv_free(&current_tag);
  return result;
}
