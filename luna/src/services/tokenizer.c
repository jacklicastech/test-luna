#include <pthread.h>
#include <sqlite3.h>
#include <czmq.h>
#include "services.h"
#include "util/base64_helpers.h"
#include "util/files.h"
#include "util/migrations.h"
#include "util/encryption_helpers.h"

typedef struct _token_t {
  int id;
  int freed;
  
  void  *sensitive_data;
  size_t sensitive_data_size;

  char  *representation;
} token_t;

static sqlite3 *db = NULL;
static zactor_t *service = NULL;
static pthread_mutex_t token_mutex = PTHREAD_MUTEX_INITIALIZER;

static int cb_assign_data(void *arg, int ncols, char **vals, char **names) {
  char *encrypted = NULL;
  size_t encrypted_size;
  char  **decrypted_data = (char   **) (((char **)arg)[0]);
  size_t *decrypted_size = (size_t *)  (((void **)arg)[1]);

  assert(ncols == 1);
  base64_decode(vals[0], &encrypted, &encrypted_size);
  int result = rsa_decrypt(encrypted, encrypted_size, decrypted_data, decrypted_size);
  free(encrypted);
  return result;
}

static int cb_assign_representation(void *arg, int ncols, char **vals, char **names) {
  char **rep = (char **) arg;
  assert(ncols == 1);
  *rep = strdup(vals[0]);
  return 0;
}

int token_data(token_id id, void **data, size_t *size) {
  int err = 1;
  pthread_mutex_lock(&token_mutex);
    *data = NULL;
    *size = 0;
    void *arg[] = { (void *) data, (void *) size };
    char *zErrMsg;
    char *query = sqlite3_mprintf("SELECT data FROM tokens WHERE id = %u", id);
    int sqlerr = sqlite3_exec(db, query, cb_assign_data, arg, &zErrMsg);
    if (sqlerr == SQLITE_OK) {
      err = 0;
    } else {
      LWARN("tokenizer: could not get data for token %u: %s", id, zErrMsg);
      sqlite3_free(zErrMsg);
    }
    sqlite3_free(query);
  pthread_mutex_unlock(&token_mutex);
  return err;
}

int token_representation(token_id id, char **representation) {
  int err = 1;
  *representation = NULL;
  pthread_mutex_lock(&token_mutex);
    char *zErrMsg;
    char *query = sqlite3_mprintf("SELECT representation FROM tokens WHERE id = %u", id);
    int sqlerr = sqlite3_exec(db, query, cb_assign_representation, representation, &zErrMsg);
    if (sqlerr == SQLITE_OK) {
      err = 0;
    } else {
      LWARN("tokenizer: could not get representation for token %u: %s", id, zErrMsg);
      sqlite3_free(zErrMsg);
    }
    sqlite3_free(query);
  pthread_mutex_unlock(&token_mutex);
  return err;
}

token_id create_token(const void *sensitive_data, size_t sensitive_data_size,
                      const char *representation) {
  token_id id = 0;

  pthread_mutex_lock(&token_mutex);
    char *encrypted = NULL;
    size_t encrypted_size = 0;
    int status = rsa_encrypt(sensitive_data, sensitive_data_size, &encrypted, &encrypted_size);
    if (status == 0) {
      char *data_b64 = base64_encode(encrypted, encrypted_size);
      free(encrypted);
      char *zErrMsg = NULL;
      char *query = sqlite3_mprintf("INSERT INTO tokens (\"data\", \"representation\") VALUES ('%q', '%q')", data_b64, representation);
      int sqlerr = sqlite3_exec(db, query, NULL, NULL, &zErrMsg);
      if (sqlerr == SQLITE_OK) {
        id = (unsigned int) sqlite3_last_insert_rowid(db);
        LDEBUG("tokenizer: created token %u (represented as: '%s')", id, representation);
      } else {
        LWARN("tokenizer: could not create token for %s: %s", representation, zErrMsg);
        if (zErrMsg) sqlite3_free(zErrMsg);
      }
      sqlite3_free(query);
      free(data_b64);
    } else {
      LWARN("tokenizer: token not created: failed to encrypt token data: %d", status);
    }
  pthread_mutex_unlock(&token_mutex);
  return id;
}

int free_token(token_id id) {
  int err = 1;
  char *zErrMsg;
  LDEBUG("tokenizier: freeing token %u", id);
  pthread_mutex_lock(&token_mutex);
    char *query = sqlite3_mprintf("DELETE FROM tokens WHERE id = %u", id);
    int sqlerr = sqlite3_exec(db, query, NULL, NULL, &zErrMsg);
    if (sqlerr == SQLITE_OK) {
      err = 0;
    } else {
      LWARN("tokenizer: could not delete token %u: %s", id, zErrMsg);
      sqlite3_free(zErrMsg);
    }
    sqlite3_free(query);
  pthread_mutex_unlock(&token_mutex);
  return err;
}

/*
 * Deletes all tokens from the token database.
 */
int nuke_tokens(void) {
  int err = 1;
  char *zErrMsg;
  LINFO("tokenizier: deleting all tokens");
  pthread_mutex_lock(&token_mutex);
    char *query = sqlite3_mprintf("DELETE FROM tokens");
    int sqlerr = sqlite3_exec(db, query, NULL, NULL, &zErrMsg);
    if (sqlerr == SQLITE_OK) {
      err = 0;
    } else {
      LWARN("tokenizer: could not nuke tokens: %s", zErrMsg);
      sqlite3_free(zErrMsg);
    }
    sqlite3_free(query);
  pthread_mutex_unlock(&token_mutex);
  return err;
}

static void cleanup_tokens() { }

static void tokens_service(zsock_t *pipe, void *args) {
  zsock_t *tokenize = zsock_new_rep(TOKENS_ENDPOINT);
  zpoller_t *poller = zpoller_new(pipe, tokenize, NULL);
  void *in = NULL;
  void *sensitive_data;
  size_t sensitive_data_size;
  char *representation;
  token_id id;
  char *db_path = find_writable_file(NULL, "tokens.sqlite3");
  int err = sqlite3_open(db_path, &db);
  if (err) {
    LERROR("tokenizer: FATAL: can't open database %s: %s", db_path, sqlite3_errmsg(db));
    sqlite3_close(db);
    free(db_path);
    zsock_signal(pipe, 1);
    zpoller_destroy(&poller);
    zsock_destroy(&tokenize);
    return;
  }
  free(db_path);
  db_path = find_readable_file(NULL, "migrations/tokens");
  if (db_path == NULL) {
    LERROR("tokenizer: FATAL: could not find migrations path for tokens database");
    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);
    return;
  }

  if (migrate(db, db_path) < 0) {
    LERROR("tokenizer: FATAL: could not migrate the tokens database");
    free(db_path);
    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);
    return;
  }
  free(db_path);
  zsock_signal(pipe, 0);
  LINFO("tokenizer: initialized");

  while ((in = zpoller_wait(poller, -1))) {
    
    if (in == pipe) {
      LDEBUG("tokenizer: received shutdown signal");
      break;
    }

    if (in == tokenize) {
      LDEBUG("tokenizer: received tokenization request");
      if (zsock_recv(tokenize, "bs", &sensitive_data, &sensitive_data_size, &representation) == -1) {
        zmsg_t *msg = zmsg_recv(tokenize);
        zmsg_destroy(&msg);
        LERROR("tokenizer: received invalid tokenization request");
        zsock_send(tokenize, "u", 0);
      } else {
        id = create_token(sensitive_data, sensitive_data_size, representation);
        free(sensitive_data);
        free(representation);
        zsock_send(tokenize, "u", id);
        LDEBUG("tokenizer: token returned: %u", id);
      }
    }
  }

  if (in == NULL) LWARN("tokens: service interrupted");
  LINFO("tokenizer: shutting down");
  sqlite3_close(db);
  zpoller_destroy(&poller);
  zsock_destroy(&tokenize);
}

int init_tokenizer_service() {
  service = zactor_new(tokens_service, NULL);
  return 0;
}

void shutdown_tokenizer_service() {
  if (service) zactor_destroy(&service);
  service = NULL;
  cleanup_tokens();
}
