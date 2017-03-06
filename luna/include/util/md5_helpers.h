#ifndef UTIL_MD5_HELPERS
#define UTIL_MD5_HELPERS

#include <openssl/md5.h> // for MD5_DIGEST_LENGTH, for convenience

/*
 * Given a NULL-terminated path to a file, reads that file and computes its
 * MD5 sum. Compares the sum to the given hex-encoded MD5, and returns 1 if
 * they match, 0 otherwise.
 *
 * `md5_hex` need not be NULL-terminated, but must be at least
 * `MD5_DIGEST_LENGTH*2` characters long (given that every byte in
 * `MD5_DIGEST_LENGTH` is represented as 2 hexadecimal characters).
 */
int md5_matches(const char *filepath, const char *md5_hex);

#endif // UTIL_MD5_HELPERS
