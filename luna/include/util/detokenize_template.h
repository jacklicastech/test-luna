#ifndef UTIL_DETOKENIZE_TEMPLATE_H
#define UTIL_DETOKENIZE_TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Detokenizes data and returns the (sensitive!) result. On input, `len`
 * should be the length of `data`. On output, `len` is changed to the length
 * of the return value.
 *
 * The return value should be freed when it is no longer needed.
 */
char *detokenize_template(const char *data, size_t *len);

/*
 * Works like `detokenize_template`, except that instead of replacing tokens
 * with sensitive data, replaces them with their human-readable
 * representation.
 */
char *humanize_template(const char *data, size_t *len);

#ifdef __cplusplus
}
#endif

#endif // UTIL_DETOKENIZE_TEMPLATE_H
