#define _GNU_SOURCE
#include "config.h"
#include <unistd.h>
#include <memory.h>
#include <stdlib.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "bindings.h"
#include "services/logger.h"

int  init_ctos_lua(lua_State *L);
void shutdown_ctos_lua(lua_State *L);
int  init_printer_lua(lua_State *L);
void shutdown_printer_lua(lua_State *L);
int  init_logger_lua(lua_State *L);
void shutdown_logger_lua(lua_State *L);
int  init_zmq_lua(lua_State *L);
void shutdown_zmq_lua(lua_State *L);
int  init_settings_lua(lua_State *L);
void shutdown_settings_lua(lua_State *L);
int  init_tokenizer_lua(lua_State *L);
void shutdown_tokenizer_lua(lua_State *L);
int  init_xml_lua(lua_State *L);
void shutdown_xml_lua(lua_State *L);
int  init_timer_lua(lua_State *L);
void shutdown_timer_lua(lua_State *L);
int  init_services_lua(lua_State *L);
void shutdown_services_lua(lua_State *L);
int  init_device_lua(lua_State *L);
void shutdown_device_lua(lua_State *L);

int init_plugin_lua_bindings(lua_State *L);
void shutdown_plugin_lua_bindings(lua_State *L);

static int env_setup = 0;

static void setup_env() {
  char *cwd;
  char lua_path[PATH_MAX], lua_cpath[PATH_MAX];

  if (env_setup == 1) return;
  env_setup = 1;
  cwd = get_current_dir_name();

  if (getenv("LUA_PATH")  != NULL) sprintf(lua_path,  "%s;", getenv("LUA_PATH"));
  else memset(lua_path,  0, sizeof(lua_path));
  if (getenv("LUA_CPATH") != NULL) sprintf(lua_cpath, "%s;", getenv("LUA_CPATH"));
  else memset(lua_cpath, 0, sizeof(lua_path));
  sprintf(lua_path  + strlen(lua_path),  DEFAULT_LUA_PATH);
  sprintf(lua_cpath + strlen(lua_cpath), DEFAULT_LUA_CPATH);
  setenv("LUA_PATH",  lua_path,  1);
  setenv("LUA_CPATH", lua_cpath, 1);
  LINFO("LUA_PATH  : %s", lua_path);
  LINFO("LUA_CPATH : %s", lua_cpath);

  free(cwd);
}

static int fatal_lua_error(lua_State *L) {
  const char *message = lua_tostring(L, -1);
  luaL_traceback(L, L, NULL, 1);
  LERROR("lua-main: fatal error occurred within lua: %s", message);
  LERROR("lua-main: %s", lua_tostring(L, -1));
  return 0;
}

static int lua_wrap_fn(const char *script, int (*loader)(lua_State *, const char *)) {
  setup_env();

  lua_State *L = NULL;
  int err = 0;

  L = luaL_newstate();
  lua_atpanic(L, fatal_lua_error);
  luaL_openlibs(L);
  init_logger_lua(L);
  init_ctos_lua(L);
  init_printer_lua(L);
  init_zmq_lua(L);
  init_tokenizer_lua(L);
  init_settings_lua(L);
  init_xml_lua(L);
  init_timer_lua(L);
  init_services_lua(L);
  init_device_lua(L);
  init_plugin_lua_bindings(L);

  lua_pushcfunction(L, fatal_lua_error);
  if (loader(L, script)) {
    LERROR("lua-main: failed to load script");
    err = 1;
  }
  else if (lua_pcall(L, 0, LUA_MULTRET, lua_gettop(L) - 1)) {
    LERROR("lua-main: failed to execute script");
    err = 2;
  }
  else {
    LDEBUG("lua-main: lua execution completed successfully");
  }

  shutdown_plugin_lua_bindings(L);
  shutdown_device_lua(L);
  shutdown_services_lua(L);
  shutdown_timer_lua(L);
  shutdown_xml_lua(L);
  shutdown_settings_lua(L);
  shutdown_tokenizer_lua(L);
  shutdown_zmq_lua(L);
  shutdown_printer_lua(L);
  shutdown_ctos_lua(L);
  shutdown_logger_lua(L);
  lua_close(L);

  return err;
}

int lua_run_script(const char *script) {
  return lua_wrap_fn(script, luaL_loadstring);
}

#undef luaL_loadfile
static int luaL_loadfile(lua_State *L, const char *fn) {
  return luaL_loadfilex(L, fn, NULL);
}

int lua_run_file(const char *filename) {
  return lua_wrap_fn(filename, luaL_loadfile);
}

