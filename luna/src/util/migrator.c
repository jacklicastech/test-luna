#define _GNU_SOURCE
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <assert.h>

#include "services/logger.h"
#include "util/migrations.h"
#include "util/utlist.h"
#include "util/uthash.h"

typedef struct migration_t {
  char filename[PATH_MAX];
  long long version;
  struct migration_t *next, *prev;
} migration_t;

typedef struct {
  long long id;
  UT_hash_handle hh;
} version_t;

static int version_cmp(void *a, void *b) {
  long long i = ((migration_t *) a)->version - ((migration_t *) b)->version;
  // don't just return i because its value may exceed the size of an int.
  if (i > 0) return 1;
  if (i < 0) return -1;
  return 0;
}

static int populate_migration_versions(void *arg, int ncols, char **col_values, char **col_names) {
  version_t **versions = (version_t **) arg;
  assert(ncols == 1);
  assert(!strcmp(col_names[0], "version"));

  version_t *version = (version_t *) calloc(1, sizeof(version_t));
  version->id = atoll(col_values[0]);

  HASH_ADD(hh, *versions, id, sizeof(long long), version);

  return 0;
}

// Returns the number of migrations performed, or a negative number if any
// failure occurs.
int migrate(sqlite3 *db, const char *path) {
  DIR *dir;
  struct dirent *entry;
  int err;
  char *errmsg = NULL;
  migration_t *migrations = NULL;
  migration_t *migration, *tmp1;
  version_t *previous_versions = NULL;
  version_t *previous, *tmp2;
  int result = 0;
  FILE *in;

  err = sqlite3_exec(db, "CREATE TABLE IF NOT EXISTS schema_migrations(version TEXT PRIMARY KEY)", NULL, NULL, &errmsg);
  if (err != SQLITE_OK) {
    LERROR("DB: failed to ensure schema_migrations exists: %s", errmsg);
    sqlite3_free(errmsg);
    result = -5;
    goto cleanup;
  }

  err = sqlite3_exec(db, "SELECT version FROM schema_migrations",
                     populate_migration_versions, &previous_versions, &errmsg);
  if (err != SQLITE_OK) {
    LERROR("DB: failed to query previous migration versions: %s", errmsg);
    sqlite3_free(errmsg);
    result = -6;
    goto cleanup;
  }

  if (!(dir = opendir(path))) {
    LERROR("DB: failed to open migration path %s", path);
    result = -1;
    goto cleanup;
  }

  if (!(entry = readdir(dir))) {
    LERROR("DB: failed to read file listing from migration path %s", path);
    closedir(dir);
    result = -2;
    goto cleanup;
  }

  do {
    if (entry->d_type != DT_DIR) {
      migration = (migration_t *) calloc(1, sizeof(migration_t));
      sprintf(migration->filename, "%s/%s", path, entry->d_name);
      migration->version = atoll(entry->d_name);
      DL_APPEND(migrations, migration);
    }
  } while ((entry = readdir(dir)));
  closedir(dir);

  DL_SORT(migrations, version_cmp);
  DL_FOREACH(migrations, migration) {
    long long version = migration->version;
    previous = NULL;
    HASH_FIND(hh, previous_versions, &version, sizeof(long long), previous);
    if (previous != NULL) {
      LDEBUG("DB: already ran migration version %lld, skipping it", version);
      continue;
    }

    in = fopen(migration->filename, "r");
    if (!in) {
      LWARN("DB: could not open migration file %s for reading", migration->filename);
      result = -3;
      break;
    }

    char *sql = strdup("BEGIN TRANSACTION; ");
    char buf[1024];
    size_t bytes = 0;
    while (!feof(in)) {
      bytes = fread(buf, 1, 1023, in);
      buf[bytes] = '\0';
      if (sql == NULL) sql = strdup(buf);
      else sql = realloc(sql, strlen(sql) + bytes + 1 + 1024);
      strcat(sql, buf);
    }
    fclose(in);

    sprintf(buf, "; INSERT INTO schema_migrations (version) VALUES ('%lld');", version);
    strcat(sql, buf);
    strcat(sql, "; COMMIT;");

    LINFO("DB: executing migration: %s", migration->filename);
    LDEBUG("%s", sql);
    err = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    free(sql);
    if (err != SQLITE_OK) {
      LERROR("DB: migration failed: %s", errmsg);
      sqlite3_free(errmsg);
      result = -4;
      break;
    }

    result++;
  }


cleanup:
  DL_FOREACH_SAFE(migrations, migration, tmp1) {
    DL_DELETE(migrations, migration);
    free(migration);
  }

  HASH_ITER(hh, previous_versions, previous, tmp2) {
    HASH_DEL(previous_versions, previous);
    free(previous);
  }

  return result;
}
