#ifndef UTIL_TOKENS_H
#define UTIL_TOKENS_H

#define TOKENS_ENDPOINT "inproc://tokens"

typedef unsigned int token_id;

#ifdef __cplusplus
  extern "C" {
#endif

int token_data(token_id id, void **data, size_t *size);
int token_representation(token_id id, char **representation);
token_id create_token(const void *sensitive_data, size_t sensitive_data_size,
                      const char *representation);
int free_token(token_id id);
int nuke_tokens(void);

int init_tokenizer_service(void);
void shutdown_tokenizer_service(void);

#ifdef __cplusplus
  }
#endif

#endif // UTIL_TOKENS_H
