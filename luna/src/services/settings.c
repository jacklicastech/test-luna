#include "config.h"
#include <assert.h>
#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <openssl/ssl.h>
#include <sqlite3.h>

#include "services.h"
#include "util/uthash.h"
#include "util/migrations.h"
#include "util/files.h"

#define PERSIST_FILENAME "settings.db"

static zactor_t *service = NULL;

static void set_default_settings(sqlite3 *db, zsock_t *bcast) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    char hash_hex[(SHA256_DIGEST_LENGTH * 2) + 1];
    SHA256_CTX sha;
    char *query = NULL;
    char *zErrMsg = NULL;
    int j, err;

    SHA256_Init(&sha);
    SHA256_Update(&sha, DEFAULT_PASSWORD, strlen(DEFAULT_PASSWORD));
    SHA256_Final(hash, &sha);
    for (j = 0; j < SHA256_DIGEST_LENGTH; j++)
        sprintf(hash_hex + (j * 2), "%02x", hash[j]);

    #define str(x) #x
    #define _str(x) str(x)
    #define set_default(k, v)                                                \
              /*LDEBUG("settings: default for '%s' is '%s'", k, v);        */\
              query = sqlite3_mprintf("INSERT OR IGNORE INTO "               \
                                      "settings(key, value) VALUES "         \
                                      "('%q', '%q')", k, v);                 \
              err = sqlite3_exec(db, query, NULL, NULL, &zErrMsg);           \
              if (err == SQLITE_OK) {                                        \
                if (bcast) zsock_send(bcast, "ss", k, v);                    \
              } else {                                                       \
                LWARN("settings: could not execute query (%s): %s",          \
                      zErrMsg, query);                                       \
                sqlite3_free(zErrMsg);                                       \
              }                                                              \
              sqlite3_free(query);

    set_default("auth.user", DEFAULT_USERNAME);
    set_default("auth.password", hash_hex);
    set_default("autoupdate.s3-bucket-name", DEFAULT_AUTOUPDATE_S3_BUCKET_NAME);
    set_default("autoupdate.s3-endpoint",    DEFAULT_AUTOUPDATE_S3_ENDPOINT);
    set_default("autoupdate.s3-prefix",      DEFAULT_AUTOUPDATE_S3_PREFIX);
    set_default("autoupdate.frequency",      DEFAULT_AUTOUPDATE_CHECK_INTERVAL);
    set_default("device.name",               DEFAULT_DEVICE_NAME);
    set_default("webserver.beacon.port",     DEFAULT_WEBSERVER_BEACON_PORT);
    set_default("webserver.beacon.enabled",  DEFAULT_WEBSERVER_BEACON_ENABLED);
    set_default("webserver.port",            _str(DEFAULT_WEBSERVER_PORT));
}

static int emit_value(void *arg, int num_columns, char **values, char **names) {
  zmsg_t *rep = (zmsg_t *) arg;
  assert(num_columns == 1 || num_columns == 2);
  if (num_columns == 1) {
    assert(!strcmp(names[0], "value"));
    if (values[0] == NULL) zmsg_addstr(rep, "");
    else zmsg_addstr(rep, values[0]);
  } else {
    assert(!strcmp(names[0], "key"));
    assert(!strcmp(names[1], "value"));
    assert(values[0] != NULL);
    zmsg_addstr(rep, values[0]);
    if (values[1] == NULL) zmsg_addstr(rep, "");
    else zmsg_addstr(rep, values[1]);
  }
  return 0;
}

static int bcast_setting_without_value(void *arg, int num_columns, char **values, char **names) {
  assert(num_columns == 1);
  assert(!strcmp(names[0], "key"));
  zsock_send((zsock_t *) arg, "ss", values[0], "");
  return 0;
}

static void settings_service(zsock_t *pipe, void *args) {
  zsock_t *changes;
  zsock_t *notify;
  zpoller_t *poller;
  zmsg_t *req, *rep;
  zframe_t *frame;
  int req_code, rep_code;
  char *key, *value;
  sqlite3 *db = NULL;
  char *errmsg = NULL;
  int err;
  int count;
  char *query = NULL;
  char *zErrMsg;
  char *migrations_path;
  char *db_path = find_writable_file(NULL, PERSIST_FILENAME);

  (void) errmsg;
  LINFO("settings: starting service");

  if (db_path == NULL) {
    LERROR("settings: FATAL: could not find writable path for settings database");
    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);
    exit(1);
    return;
  }

  err = sqlite3_open(db_path, &db);
  if (err) {
    LERROR("settings: FATAL: can't open database: %s", sqlite3_errmsg(db));
    sqlite3_close(db);
    free(db_path);
    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);
    return;
  }
  free(db_path);

  // run any pending migrations, abort fatally if migrations fail
  migrations_path = find_readable_file(NULL, "migrations/settings");
  if (migrations_path == NULL) {
    LERROR("settings: FATAL: could not find migrations path for settings database");
    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);
    return;
  }

  if (migrate(db, migrations_path) < 0) {
    LERROR("settings: FATAL: could not migrate the settings database");
    free(migrations_path);
    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);
    return;
  }
  free(migrations_path);

  set_default_settings(db, NULL);
  changes = zsock_new_rep(SETTINGS_ENDPOINT);
  notify  = zsock_new_pub(SETTINGS_CHANGED_ENDPOINT);
  poller  = zpoller_new(pipe, changes, NULL);
  zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);

  while (1) {
    void *in = zpoller_wait(poller, -1);
    if (!in) {
      LWARN("settings: service interrupted: %s", zmq_strerror(zmq_errno()));
      break;
    }

    if (in == pipe) {
      LINFO("settings: received shutdown signal");
      break;
    }

    req = zmsg_recv(changes);

    if (!(frame = zmsg_pop(req))) {
      LWARN("settings: BUG: received empty message");
      goto msg_done;
    }

    req_code = * (int *) zframe_data(frame);
    zframe_destroy(&frame);
    switch(req_code) {
      case SETTINGS_PURGE:
        errmsg = NULL;

        query = sqlite3_mprintf("SELECT key FROM settings");
        err = sqlite3_exec(db, query, bcast_setting_without_value, notify, &zErrMsg);
        if (err != SQLITE_OK)
          LWARN("settings: could not execute query (%s): %s", zErrMsg, query);
        sqlite3_free(zErrMsg);
        sqlite3_free(query);

        query = sqlite3_mprintf("DELETE FROM settings");
        err = sqlite3_exec(db, query, emit_value, rep, &zErrMsg);
        if (err != SQLITE_OK) {
          LWARN("settings: could not execute query (%s): %s", zErrMsg, query);
          sqlite3_free(zErrMsg);
        }
        sqlite3_free(query);
        set_default_settings(db, notify);
        zsock_signal(changes, SETTINGS_RESPONSE_OK);
        break;
      case SETTINGS_GET:
        rep_code = SETTINGS_RESPONSE_OK;
        rep = zmsg_new();
        frame = zframe_new(&rep_code, sizeof(int));
        zmsg_append(rep, &frame);
        // LDEBUG("with %d arguments", zmsg_size(req));
        if (zmsg_size(req) > 0) {
          while (zmsg_size(req) > 0) {
            count = zmsg_size(rep);
            key = zmsg_popstr(req);
            LDEBUG("settings: Getting setting %s", key);
            errmsg = NULL;
            query = sqlite3_mprintf("SELECT value FROM settings WHERE key = '%q'", key);
            err = sqlite3_exec(db, query, emit_value, rep, &zErrMsg);
            if (err == SQLITE_OK) {
              if (zmsg_size(rep) == count) {
                LDEBUG("settings: setting not found: %s", key);
                zmsg_addstr(rep, "");
              }
            } else {
              LWARN("settings: could not execute query (%s): %s", zErrMsg, query);
              sqlite3_free(zErrMsg);
              zmsg_addstr(rep, "");
            }
            sqlite3_free(query);
            free(key);
          }
        } else {
          errmsg = NULL;
          query = sqlite3_mprintf("SELECT key, value FROM settings");
          err = sqlite3_exec(db, query, emit_value, rep, &zErrMsg);
          if (err != SQLITE_OK) {
            LWARN("settings: could not execute query (%s): %s", zErrMsg, query);
            sqlite3_free(zErrMsg);
          }
          sqlite3_free(query);
        }
        zmsg_send(&rep, changes);
        break;
      case SETTINGS_DEL:
        if (zmsg_size(req) == 0) {
          LERROR("settings: BUG: request for DEL included no keys");
          zsock_signal(changes, SETTINGS_RESPONSE_ERROR);
          break;
        }
        while (zmsg_size(req) > 0) {
          key = zmsg_popstr(req);
          LDEBUG("settings: deleting setting '%s'", key);
          query = sqlite3_mprintf("DELETE FROM settings WHERE key = '%q'", key);
          err = sqlite3_exec(db, query, NULL, NULL, &zErrMsg);
          if (err != SQLITE_OK) {
            LWARN("settings: could not execute query (%s): %s", zErrMsg, query);
            sqlite3_free(zErrMsg);
          } else {
            zsock_send(notify, "ss", key, "");
          }
          free(key);
          sqlite3_free(query);
          zsock_signal(changes, SETTINGS_RESPONSE_OK);
        }
        break;
      case SETTINGS_SET:
        if (zmsg_size(req) == 0) {
          LERROR("settings: BUG: request for SET included no keys");
          zsock_signal(changes, SETTINGS_RESPONSE_ERROR);
          break;
        }
        if (zmsg_size(req) % 2 != 0) {
          LERROR("settings: BUG: there is not a value for every key");
          zsock_signal(changes, SETTINGS_RESPONSE_ERROR);
          break;
        }
        while (zmsg_size(req) > 0) {
          key = zmsg_popstr(req);
          value = zmsg_popstr(req);
          LDEBUG("settings: changing value of setting '%s'", key);
          query = sqlite3_mprintf("INSERT OR REPLACE INTO settings (\"key\", \"value\") VALUES ('%q', '%q')", key, value);
          err = sqlite3_exec(db, query, NULL, NULL, &zErrMsg);
          if (err == SQLITE_OK) {
            zsock_send(notify, "ss", key, value);
          } else {
            LWARN("settings: could not execute query (%s): %s", zErrMsg, query);
            sqlite3_free(zErrMsg);
          }
          sqlite3_free(query);
          free(key);
          free(value);
        }
        zsock_signal(changes, SETTINGS_RESPONSE_OK);
        break;
      default:
        LERROR("settings: BUG: request was neither GET nor SET: %d", req_code);
        zsock_signal(changes, SETTINGS_RESPONSE_ERROR);
    }

msg_done:
    zmsg_destroy(&req);
  }

  LINFO("settings: shutting down service");
  sqlite3_close(db);
  zsock_destroy(&changes);
  zsock_destroy(&notify);
  zpoller_destroy(&poller);
  LINFO("settings: service shutdown complete");
}

int init_settings_service(void) {
  if (service) return 1;
  service = zactor_new(settings_service, NULL);
  if (!service) return 1;
  return 0;
}

void shutdown_settings_service(void) {
  if (service) {
    zactor_destroy(&service);
    service = NULL;
  }
}

/*
Example:
    settings_get(sock, 2, "setting1", "setting2", &s1val, &s2val);

Don't send a zero count.
*/
int settings_get(zsock_t *getset, int num, ...) {
  va_list ap;
  zmsg_t *msg;
  zframe_t *frame;
  int code = SETTINGS_GET;
  int i;

  va_start(ap, num);
  assert(num > 0);

  msg = zmsg_new();
  frame = zframe_new(&code, sizeof(code));
  zmsg_append(msg, &frame);
  for (i = 0; i < num; i++) {
    const char *key = va_arg(ap, char *);
    zmsg_addstr(msg, key);
  }
  zmsg_send(&msg, getset);

  msg = zmsg_recv(getset);
  frame = zmsg_pop(msg);
  code = * (int *) zframe_data(frame);
  zframe_destroy(&frame);
  assert(code == SETTINGS_RESPONSE_OK);
  assert(zmsg_size(msg) == (unsigned) num);

  for (i = 0; i < num; i++) {
    char ** out = va_arg(ap, char **);
    *out = zmsg_popstr(msg);
  }

  zmsg_destroy(&msg);
  va_end(ap);

  return 0;
}

zmsg_t *settings_getall(zsock_t *getset) {
  int code = SETTINGS_GET;
  zmsg_t *msg = zmsg_new();
  zframe_t *frame = zframe_new(&code, sizeof(code));
  zmsg_append(msg, &frame);
  zmsg_send(&msg, getset);
  msg = zmsg_recv(getset);
  frame = zmsg_pop(msg);
  assert(* (int *) zframe_data(frame) == SETTINGS_RESPONSE_OK);
  zframe_destroy(&frame);
  return msg;
}

/*
Example:
    settings_get(sock, 2, "setting1", &s1val, "setting2", &s2val);

Don't send a zero count.
*/
int settings_set(zsock_t *getset, int num, ...) {
  va_list ap;
  zmsg_t *msg;
  zframe_t *frame;
  char *value;
  int code = SETTINGS_SET;
  int i;

  assert(num > 0);
  msg = zmsg_new();
  frame = zframe_new(&code, sizeof(code));
  zmsg_append(msg, &frame);

  va_start(ap, num);
  for (i = 0; i < num; i++) {
    zmsg_addstr(msg, va_arg(ap, char *));
    value = va_arg(ap, char *);
    if (value != NULL) zmsg_addstr(msg, value);
    else zmsg_addstr(msg, "");
  }
  va_end(ap);

  zmsg_send(&msg, getset);

  return zsock_wait(getset);
}

int settings_del(zsock_t *getset, int num, ...) {
  va_list ap;
  zmsg_t *msg;
  zframe_t *frame;
  //char *value;
  int code = SETTINGS_DEL;
  int i;

  assert(num > 0);
  msg = zmsg_new();
  frame = zframe_new(&code, sizeof(code));
  zmsg_append(msg, &frame);

  va_start(ap, num);
  for (i = 0; i < num; i++) {
    zmsg_addstr(msg, va_arg(ap, char *));
  }
  va_end(ap);

  zmsg_send(&msg, getset);

  return zsock_wait(getset);
}

int settings_purge(zsock_t *sock) {
  int code = SETTINGS_PURGE;
  zmsg_t *msg = zmsg_new();
  zframe_t *frame = zframe_new(&code, sizeof(code));
  zmsg_append(msg, &frame);
  zmsg_send(&msg, sock);
  return zsock_wait(sock);
}
