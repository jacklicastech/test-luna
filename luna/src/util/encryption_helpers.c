#include "config.h"
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <memory.h>

#include "services/logger.h"
#include "util/files.h"
#include "util/encryption_helpers.h"

#define AES_KEYLEN 256

static EVP_PKEY *public_key  = NULL;
static EVP_PKEY *private_key = NULL;

// At least by having a passphrase we force a malicious user to analyze our
// application instead of just the key and database.
#define PRIVATE_KEY_PASSPHRASE DECRYPTION_KEY_PASSPHRASE

/*
 * Generates a random key and IV, then encrypts `data` and places the result
 * in `out_encrypted`. The IV is placed in `out_iv`. The encryption key is
 * placed in `out_key`. All of these should be freed when no longer needed.
 *
 * Returns 0 on success. If any failure occurs, the arguments will not be
 * modified.
 */
int aes256cbc_encrypt(const char *data,     size_t data_size,
                      char **out_encrypted, size_t *out_encrypted_size,
                      char **out_key,       size_t *out_key_size,
                      char **out_iv,        size_t *out_iv_size) {
  int err = -1;

  EVP_CIPHER_CTX *ctx      = NULL;
  unsigned char *key       = NULL;
  unsigned char *iv        = NULL;
  unsigned char *encrypted = NULL;
  size_t block_size = 0, encrypted_size = 0;

  key       = (unsigned char *) malloc(AES_KEYLEN / 8);
  iv        = (unsigned char *) malloc(AES_KEYLEN / 8);
  encrypted = (unsigned char *) malloc(data_size + AES_BLOCK_SIZE);
  if (!key || !iv || !encrypted) goto aes256cbc_encrypt_fail;

  ctx       = (EVP_CIPHER_CTX *) malloc(sizeof(EVP_CIPHER_CTX));
  if (!ctx) goto aes256cbc_encrypt_fail;
  EVP_CIPHER_CTX_init(ctx);

  if (!RAND_bytes(key, AES_KEYLEN / 8))  goto aes256cbc_encrypt_fail;
  if (!RAND_bytes(iv,  AES_KEYLEN / 8))  goto aes256cbc_encrypt_fail;

  if (!EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, key, iv))
    goto aes256cbc_encrypt_fail;
  if (!EVP_EncryptUpdate(ctx, encrypted, (int *) &block_size, (unsigned char *) data, data_size))
    goto aes256cbc_encrypt_fail;
  encrypted_size += block_size;
  if (!EVP_EncryptFinal_ex(ctx, encrypted + encrypted_size, (int *) &block_size))
    goto aes256cbc_encrypt_fail;
  encrypted_size += block_size;

  err                 = 0;
  *out_encrypted      = (char *) encrypted;
  *out_key            = (char *) key;
  *out_iv             = (char *) iv;
  *out_encrypted_size = encrypted_size;
  *out_key_size       = AES_KEYLEN / 8;
  *out_iv_size        = AES_KEYLEN / 8;
  goto aes256cbc_encrypt_done;

aes256cbc_encrypt_fail:
  if (encrypted) free(encrypted);
  if (key)       free(key);
  if (iv)        free(iv);

aes256cbc_encrypt_done:
  if (ctx) {
    EVP_CIPHER_CTX_cleanup(ctx);
    free(ctx);
  }
  return err;
}

/*
 * Decrypts an encrypted message acccording to the encryption key and IV
 * specified. Places output into `out_data`, which should be freed when no
 * longer needed.
 *
 * Returns 0 on success. If a failure occurs, the arguments are not changed.
 */
int aes256cbc_decrypt(char *encrypted, size_t encrypted_size,
                      char *key,       size_t key_size,
                      char *iv,        size_t iv_size,
                      char **out_data, size_t *out_data_size) {
  int err = -1;
  EVP_CIPHER_CTX *ctx = NULL;
  unsigned char *data = NULL;
  size_t data_size = 0, block_size = 0;

  data = (unsigned char *) malloc(encrypted_size);
  if (!data) goto aes256cbc_decrypt_fail;

  ctx = (EVP_CIPHER_CTX *) malloc(sizeof(EVP_CIPHER_CTX));
  if (!ctx) goto aes256cbc_decrypt_fail;
  EVP_CIPHER_CTX_init(ctx);

  if (!EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (unsigned char *) key, (unsigned char *) iv))
    goto aes256cbc_decrypt_fail;
  if (!EVP_DecryptUpdate(ctx, (unsigned char *) data, (int *) &block_size, (const unsigned char *) encrypted, (int) encrypted_size))
    goto aes256cbc_decrypt_fail;
  data_size += block_size;
  if (!EVP_DecryptFinal_ex(ctx, (unsigned char *) data + data_size, (int *) &block_size))
    goto aes256cbc_decrypt_fail;
  data_size += block_size;

  err            = 0;
  *out_data      = (char *) data;
  *out_data_size = data_size;
  goto aes256cbc_decrypt_done;

aes256cbc_decrypt_fail:
  if (data) free(data);

aes256cbc_decrypt_done:
  if (ctx) {
    EVP_CIPHER_CTX_cleanup(ctx);
    free(ctx);
  }
  return err;
}

static int init_rsa_keys(void) {
  int err = -1;
  char *pub_filename = NULL, *pri_filename = NULL;
  FILE *pub_file = NULL, *pri_file = NULL;

  // return success if they've already been (successfully) loaded
  if (private_key && public_key) return 0;

  pub_filename = find_readable_file(NULL, "encrypt.pem");
  pri_filename = find_readable_file(NULL, "decrypt.pem");
  if (!pub_filename || !pri_filename) {
    LWARN("could not expand one or both of public and private key filenames");
    goto init_rsa_keys_fail;
  }

  pub_file = fopen(pub_filename, "r");
  if (!pub_file) {
    LWARN("could not open public key file at %s", pub_filename);
    goto init_rsa_keys_fail;
  }

  pri_file = fopen(pri_filename, "r");
  if (!pri_file) {
    LWARN("could not open private key file at %s", pri_filename);
    goto init_rsa_keys_fail;
  }

  public_key  = PEM_read_PUBKEY(pub_file, NULL, NULL, NULL);
  if (!public_key) {
    LWARN("could not parse public key from file at %s", pub_filename);
    ERR_print_errors_fp(stderr);
    goto init_rsa_keys_fail;
  }

  private_key = PEM_read_PrivateKey(pri_file, NULL, NULL, PRIVATE_KEY_PASSPHRASE);
  if (!private_key) {
    LWARN("could not parse private key from file at %s", pri_filename);
    ERR_print_errors_fp(stderr);
    goto init_rsa_keys_fail;
  }

  err = 0;
  goto init_rsa_keys_done;

// prevent leaking memory on subsequent attempts if loading keys should fail
init_rsa_keys_fail:
  if (private_key)  free(private_key);
  if (public_key)   free(public_key);
  private_key = NULL;
  public_key  = NULL;

init_rsa_keys_done:
  if (pub_filename) free(pub_filename);
  if (pri_filename) free(pri_filename);
  if (pub_file)     fclose(pub_file);
  if (pri_file)     fclose(pri_file);
  return err;
}

int init_encryption(void) {
  SSL_library_init();
  OpenSSL_add_all_algorithms(); /* load & register all cryptos, etc. */
  SSL_load_error_strings();     /* load all error messages */
  return init_rsa_keys();
}

/*
 * Encrypts a message using an RSA public key.
 *
 * Note: this algorithm actually uses AES-256-CBC under the hood, with a
 * randomly generated AES key and IV. The AES key is then encrypted using the
 * RSA public key. The encrypted AES key and IV are prepended to the encrypted
 * message, so that they can be recovered by a subsequent call to
 * `rsa_decrypt`.
 *
 * Places the encrypted message, IV and key into `out`. This result should
 * be freed when no longer needed.
 *
 * Returns 0 on success. If any failure occurs, arguments are not modified.
 */
int rsa_encrypt(const char *data, size_t data_size, char **out, size_t *out_size) {
/*
  *out_size = data_size;
  *out = malloc(data_size);
  memcpy(*out, data, data_size);
  return 0;
*/
  int            err = -1;
  size_t         encrypted_size = 0, block_size = 0;
  int            encryption_key_size = 0;
  unsigned char  *encryption_key = NULL;
  unsigned char  *iv = NULL;
  unsigned char  *encrypted = NULL;
  unsigned char  *combined = NULL;
  size_t         combined_size;
  EVP_CIPHER_CTX *ctx = NULL;

  if (init_rsa_keys()) goto rsa_encrypt_fail;

  encryption_key = (unsigned char *) malloc(EVP_PKEY_size(public_key));
  iv             = (unsigned char *) malloc(EVP_MAX_IV_LENGTH);
  encrypted      = (unsigned char *) malloc(data_size + EVP_MAX_IV_LENGTH);
  if (!encryption_key || !iv || !encrypted) goto rsa_encrypt_fail;

  ctx = (EVP_CIPHER_CTX *) malloc(sizeof(EVP_CIPHER_CTX));
  if (!ctx) goto rsa_encrypt_fail;
  EVP_CIPHER_CTX_init(ctx);

  if (!EVP_SealInit(ctx, EVP_aes_256_cbc(), &encryption_key, &encryption_key_size,
                    iv, &public_key, 1))
    goto rsa_encrypt_fail;
  if (!EVP_SealUpdate(ctx, encrypted, (int *) &block_size, (const unsigned char *) data, (int) data_size))
    goto rsa_encrypt_fail;
  encrypted_size += block_size;
  if (!EVP_SealFinal(ctx, encrypted + encrypted_size, (int *) &block_size))
    goto rsa_encrypt_fail;
  encrypted_size += block_size;

  // add in the IV and encrypted key
  combined_size = EVP_MAX_IV_LENGTH
                + sizeof(int) + encryption_key_size
                + encrypted_size;
  combined = (unsigned char *) malloc(combined_size);
  memcpy(combined, iv, EVP_MAX_IV_LENGTH);
  memcpy(combined + EVP_MAX_IV_LENGTH, &encryption_key_size, sizeof(int));
  memcpy(combined + EVP_MAX_IV_LENGTH + sizeof(int),
         encryption_key, encryption_key_size);
  memcpy(combined + EVP_MAX_IV_LENGTH + sizeof(int) + encryption_key_size,
         encrypted, encrypted_size);

  // done
  err       = 0;
  *out      = (char *) combined;
  *out_size = combined_size;
  goto rsa_encrypt_done;

rsa_encrypt_fail:
  if (combined) free(combined);

rsa_encrypt_done:
  if (encryption_key) free(encryption_key);
  if (iv)             free(iv);
  if (encrypted)      free(encrypted);
  if (ctx) {
    EVP_CIPHER_CTX_cleanup(ctx);
    free(ctx);
  }
  return err;
}

/*
 * Decrypts a message using an RSA public key.
 *
 * Note: this function is purpose-built to be the counterpart of
 * `rsa_encrypt`. As such it won't work with data encrypted by other
 * functions. Specifically, it expects to find the IV and encrypted key in
 * addition to the data to be encrypted.
 *
 * Places the decrypted message into `out`. This result should be freed when
 * no longer needed.
 *
 * Returns 0 on success. If any failure occurs, arguments are not modified.
 */
int rsa_decrypt(const char *encrypted, size_t encrypted_size,
                char **out, size_t *out_size) {
/*
  *out_size = encrypted_size;
  *out = malloc(encrypted_size);
  memcpy(*out, encrypted, encrypted_size);
  return 0;
*/
  int err = -1;
  size_t         data_size = 0, block_size = 0;
  int            encryption_key_size = 0;
  unsigned char  *encryption_key = NULL;
  unsigned char  *iv = NULL;
  EVP_CIPHER_CTX *ctx = NULL;
  unsigned char  *data = NULL;

  if (init_rsa_keys()) goto rsa_decrypt_fail;

  // unpack encryption key and IV from blob
  iv = (unsigned char *) malloc(EVP_MAX_IV_LENGTH);
  if (!iv) goto rsa_decrypt_fail;

  memcpy(iv, encrypted, EVP_MAX_IV_LENGTH);
  encrypted += EVP_MAX_IV_LENGTH;
  encrypted_size -= EVP_MAX_IV_LENGTH;

  encryption_key_size = *(int *)encrypted;
  encrypted += sizeof(int);
  encrypted_size -= sizeof(int);

  encryption_key = (unsigned char *) malloc(encryption_key_size);
  if (!encryption_key) goto rsa_decrypt_fail;
  memcpy(encryption_key, encrypted, encryption_key_size);
  encrypted += encryption_key_size;
  encrypted_size -= encryption_key_size;

  // what remains is just the encrypted message, so a pretty standard decrypt
  // process now
  ctx = (EVP_CIPHER_CTX *) malloc(sizeof(EVP_CIPHER_CTX));
  if (!ctx) goto rsa_decrypt_fail;
  EVP_CIPHER_CTX_init(ctx);

  data = (unsigned char *) malloc(encrypted_size + EVP_MAX_IV_LENGTH);
  if (!data) goto rsa_decrypt_fail;

  if (!EVP_OpenInit(ctx, EVP_aes_256_cbc(), encryption_key, encryption_key_size,
                    iv, private_key))
    goto rsa_decrypt_fail;
  if (!EVP_OpenUpdate(ctx, data, (int *) &block_size, (const unsigned char *) encrypted, (int) encrypted_size))
    goto rsa_decrypt_fail;
  data_size = block_size;
  if (!EVP_OpenFinal(ctx, data + data_size, (int *) &block_size))
    goto rsa_decrypt_fail;
  data_size += block_size;

  err       = 0;
  *out      = (char *) data;
  *out_size = data_size;
  goto rsa_decrypt_done;

rsa_decrypt_fail:
  if (data) free(data);

rsa_decrypt_done:
  if (encryption_key) free(encryption_key);
  if (iv)             free(iv);
  if (ctx) {
    EVP_CIPHER_CTX_cleanup(ctx);
    free(ctx);
  }
  return err;
}

