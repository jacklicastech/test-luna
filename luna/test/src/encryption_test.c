#include <stdlib.h>
#include <memory.h>
#include <stdio.h>

#include "util/encryption_helpers.h"
#include "services/logger.h"

void xxd(const char *data, size_t size) {
  int i, j;
  #define line_width 80 / 3 /* times 3, for hex and space */
  char line[80];

  for (i = 0; i < size; i += line_width) {
    memset(line, 0, sizeof(line));
    for (j = 0; j < line_width && i + j < size; j++) {
      sprintf(line + (j * 3), "%02x ", (unsigned char) data[i + j]);
    }
    LDEBUG("    %s", line);
  }
}

int test_rsa_encryption_decryption(void) {
  const char *message = "The quick brown fox jumped over the lazy dog.";
  char *encrypted = NULL;
  char *decrypted = NULL;
  size_t encrypted_size = 0;
  size_t decrypted_size = 0;
  int result = 0;

  LINFO("RSA");
  LINFO("  text to be encrypted: %s", message);
  xxd(message, strlen(message) + 1);

  result = rsa_encrypt(message, strlen(message) + 1, &encrypted, &encrypted_size);
  if (result) {
    LERROR("  FAIL: encryption failed with %d", result);
    return 1;
  }

  if (!strncmp(message, encrypted, encrypted_size)) {
    LERROR("  FAIL: encrypted message matches input, so nothing happened!");
    return 1;
  }

  LINFO("  encrypted message: %d bytes", (int) encrypted_size);
  xxd(encrypted, encrypted_size);

  result = rsa_decrypt(encrypted, encrypted_size, &decrypted, &decrypted_size);
  if (result) {
    LERROR("  FAIL: decryption failed with %d", result);
    return 1;
  }

  if (strncmp(message, decrypted, decrypted_size)) {
    LERROR("  FAIL: decrypted message does not match input");
    return 1;
  }

  if (encrypted) free(encrypted);
  if (decrypted) free(decrypted);

  return 0;
}

int test_aes256cbc_encryption_decryption(void) {
  const char *message = "The quick brown fox jumped over the lazy dog.";
  char *encrypted = NULL;
  char *key = NULL, *iv = NULL;
  char *decrypted = NULL;
  size_t encrypted_size = 0, key_size = 0, iv_size = 0;
  size_t decrypted_size = 0;
  int result = 0;

  LINFO("AES-256-CBC");
  LINFO("  text to be encrypted: %s", message);
  xxd(message, strlen(message) + 1);

  result = aes256cbc_encrypt(message, strlen(message) + 1,
                             &encrypted, &encrypted_size,
                             &key,       &key_size,
                             &iv,        &iv_size);
  if (result) {
    LERROR("  FAIL: encryption failed with %d", result);
    return 1;
  }

  if (!strncmp(message, encrypted, encrypted_size)) {
    LERROR("  FAIL: encrypted message matches input, so nothing happened!");
    return 1;
  }

  LINFO("  encryption key: %d bytes", (int) key_size);
  xxd(key, key_size);
  LINFO("  IV: %d bytes", (int) iv_size);
  xxd(iv, iv_size);
  LINFO("  encrypted message: %d bytes", (int) encrypted_size);
  xxd(encrypted, encrypted_size);

  result = aes256cbc_decrypt(encrypted,  encrypted_size,
                             key,        key_size,
                             iv,         iv_size,
                             &decrypted, &decrypted_size);
  if (result) {
    LERROR("  FAIL: decryption failed with %d", result);
    return 1;
  }

  if (strncmp(message, decrypted, decrypted_size)) {
    LERROR("  FAIL: decrypted message does not match input");
    return 1;
  }

  if (encrypted) free(encrypted);
  if (decrypted) free(decrypted);

  return 0;
}

int main() {
  int err;

  init_logger_service(LOG_LEVEL_DEBUG);
  init_encryption();

  err = test_rsa_encryption_decryption() ||
        test_aes256cbc_encryption_decryption();

  shutdown_logger_service();
  return err;
}
