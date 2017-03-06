#define  _GNU_SOURCE
#include "config.h"
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/limits.h>
#include "bindings.h"

#define TEST_BASIC_SQL "sqlite = require('lsqlite3')"                                "\n" \
                       "db = sqlite.open('lua_test_db.sqlite3')"                     "\n" \
                       "assert(db:migrate('migrations/lua_sqlite3_test') >= 0)"      "\n" \
                       "rs = {}"                                                     "\n" \
                       "for row in db:rows('select name from widgets') do"           "\n" \
                         "rs[#rs + 1] = row"                                         "\n" \
                       "end"                                                         "\n" \
                       "assert(#rs == 1)"                                            "\n" \
                       "assert(rs[1][1] == 'dongle')"                                "\n" \
                       "db:close(); os.remove('lua_test_db.sqlite3')"                "\n" \
                       "db = sqlite.open('lua_test_db.sqlite3')"                     "\n" \
                       "success, err = pcall(db.rows, db, 'select name from widgets') \n" \
                       "assert(success == false) -- missing table"

int main(int argc, char **argv) {
  int err = 0;

  unlink("lua_test_db.sqlite3");
  if ((err = lua_run_script(TEST_BASIC_SQL))) goto done;

done:
  return err;
}
