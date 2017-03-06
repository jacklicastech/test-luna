#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include "services.h"

#define Red     "\x1b[31m"
#define Green   "\x1b[32m"
#define Regular "\x1b[0m"
#define Assert(x)                                                            \
  if (!(x)) { LDEBUG(Red "Assert: FAIL: %s" Regular, #x); assert(0); }         \
  else { LDEBUG(Green "Assert: %s" Regular, #x); }
#define Assert2(x, y) \
  if (!(x)) { LDEBUG(Red "Assert: FAIL: (%s) : \%{%s}" Regular, #x, y); assert(0); } \
  else { LDEBUG(Green "Assert: %s" Regular, #x); }

static zsock_t *settings = NULL;

/*
 * Several settings are assigned by default on first-run. Ensure that this
 * process happens. (We're not testing the defaults themselves, only that a
 * set of defaults was enumerated and assigned.)
 */
static void test_default_settings() {
  char *val = NULL;
  settings_get(settings, 1, "device.name", &val);
  Assert(val != NULL);
  Assert(strlen(val) > 0);
  free(val);
}

/*
 * If a setting key doesn't exist, fetching it should return an empty string.
 */
static void test_get_missing_setting() {
  char *val = NULL;
  settings_get(settings, 1, "missing.setting.name", &val);
  Assert(val != NULL);
  Assert(strlen(val) == 0);
  free(val);
}

/*
 * Ensure a setting remains in memory after assignment.
 */
static void test_set_and_retrieve_setting() {
  const char *set = "one";
  char *get = NULL;
  settings_set(settings, 1, "setting.name", set);
  settings_get(settings, 1, "setting.name", &get);
  Assert(!strcmp(set, get));
  free(get);
}

/*
 * Ensure a setting can be stored with an apostrophe
 */
static void test_set_apostrophe() {
  const char *set = "o'neil";
  char *get = NULL;
  settings_set(settings, 1, "not'creative", set);
  settings_get(settings, 1, "not'creative", &get);
  Assert(!strcmp(set, get));
  free(get);
}

/*
 * Ensure a setting can be deleted.
 */
static void test_delete_setting() {
  const char *set = "one";
  char *get = NULL;
  settings_set(settings, 1, "setting.name", set);
  settings_del(settings, 1, "setting.name");
  settings_get(settings, 1, "setting.name", &get);
  Assert(!strcmp(get, ""));
  free(get);
}

/*
 * Ensure a setting can be set to NULL.
 */
static void test_set_to_null() {
  const char *set = "one";
  char *get = NULL;
  settings_set(settings, 1, "setting.name", set);
  settings_set(settings, 1, "setting.name", NULL);
  settings_get(settings, 1, "setting.name", &get);
  Assert(!strcmp(get, ""));
  free(get);
}

/*
 * Ensure settings are saved across service shutdown & restart
 */
static void test_auto_persistence() {
  char *get = NULL;
  settings_set(settings, 1, "persist-on-shutdown", "1");
  zsock_destroy(&settings);
  shutdown_settings_service();
  assert(!init_settings_service());
  settings = zsock_new_req(SETTINGS_ENDPOINT);
  settings_get(settings, 1, "persist-on-shutdown", &get);
  assert(!strcmp(get, "1"));
  free(get);
}

int main(int argc, char **argv) {
  int err = 0;

  // To make sure the test run is clean, delete the settings file if it is
  // present before starting the service.
  unlink("settings.db");

  if ((err = init_logger_service(LOG_LEVEL_DEBUG))) goto shutdown;
  if ((err = init_settings_service()))              goto shutdown;

  settings = zsock_new_req(SETTINGS_ENDPOINT);
  assert(settings);

  test_default_settings();
  test_get_missing_setting();
  test_set_and_retrieve_setting();
  test_auto_persistence();
  test_delete_setting();
  test_set_to_null();
  test_set_apostrophe();
  
shutdown:
  unlink("settings.db");
  zsock_destroy(&settings);
  shutdown_settings_service();
  shutdown_logger_service();

  return err;
}
