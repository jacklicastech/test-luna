#ifndef UTIL_MIGRATIONS_H

#include <sqlite3.h>

#ifdef __CPLUSPLUS
extern "C" {
#endif

int migrate(sqlite3 *db, const char *path);

#ifdef __CPLUSPLUS
}
#endif

#endif // UTIL_MIGRATIONS_H
