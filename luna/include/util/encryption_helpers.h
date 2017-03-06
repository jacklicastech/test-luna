#ifndef UTIL_ENCRYPTION_HELPERS_H
#define UTIL_ENCRYPTION_HELPERS_H

/*
 * Initializes OpenSSL and related state. Call once during application
 * startup. Returns 0 on success.
 */
int init_encryption(void);

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
                      char **out_iv,        size_t *out_iv_size);

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
                      char **out_data, size_t *out_data_size);

/*
 * Encrypts a message using the RSA public key in the file 'encrypt.pem'.
 *
 * Note: this algorithm actually uses AES-256-CBC under the hood, with a
 * randomly generated encryption key and IV. The AES key is then encrypted
 * using the RSA public key. The encrypted key and IV are prepended to the
 * encrypted message, so that they can be recovered by a subsequent call to
 * `rsa_decrypt`.
 *
 * Places the encrypted message, IV and key into `out`. This result should
 * be freed when no longer needed.
 *
 * Returns 0 on success. If any failure occurs, arguments are not modified.
 */
int rsa_encrypt(const char *data, size_t data_size, char **out, size_t *out_size);

/*
 * Decrypts a message using the RSA public key in the file 'decrypt.pem'.
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
                char **out, size_t *out_size);

#endif // UTIL_ENCRYPTION_HELPERS_H
