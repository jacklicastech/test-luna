#include <openssl/crypto.h>
#include <pthread.h>

static pthread_mutex_t   *ssl_lock_array = NULL;

static void lock_callback(int mode, int type, char *file, int line) {
  (void)file;
  (void)line;
  if(mode & CRYPTO_LOCK) {
    pthread_mutex_lock(&(ssl_lock_array[type]));
  }
  else {
    pthread_mutex_unlock(&(ssl_lock_array[type]));
  }
}
 
static unsigned long thread_id(void) {
  return (unsigned long) pthread_self();
}
 
void init_ssl_locks(void) {
  int i;
 
  ssl_lock_array=(pthread_mutex_t *)OPENSSL_malloc(CRYPTO_num_locks() *
                                            sizeof(pthread_mutex_t));
  for(i=0; i<CRYPTO_num_locks(); i++) {
    pthread_mutex_init(&(ssl_lock_array[i]), NULL);
  }
 
  CRYPTO_set_id_callback((unsigned long (*)())thread_id);
  CRYPTO_set_locking_callback((void (*)())lock_callback);
}
 
void shutdown_ssl_locks(void) {
  int i;
 
  if (ssl_lock_array) {
    CRYPTO_set_locking_callback(NULL);
    for(i=0; i<CRYPTO_num_locks(); i++)
      pthread_mutex_destroy(&(ssl_lock_array[i]));
   
    OPENSSL_free(ssl_lock_array);
  }
}
