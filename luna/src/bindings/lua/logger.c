#define LUA_LIB

#include "config.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdint.h>

#include "services/logger.h"

/*
 * Log output at the INSECURE level. No effect if the current log level
 * restricts insecure messages, or if this build was compiled with insecure
 * log messages disabled (e.g. in production).
 * 
 * Examples:
 * 
 *     logger = require("logger")
 *     logger.insecure("text to output")
 */
static int logger_insecure(lua_State *L) {
  const char *str = lua_tostring(L, 1);
  LINSEC("lua: %s", str);
  return 0;
}

/*
 * Log output at the TRACE level. No effect if the current log level restricts
 * trace messages.
 * 
 * Examples:
 * 
 *     logger = require("logger")
 *     logger.trace("text to output")
 */
static int logger_trace(lua_State *L) {
  const char *str = lua_tostring(L, 1);
  LTRACE("lua: %s", str);
  return 0;
}

/*
 * Log output at the DEBUG level. No effect if the current log level restricts
 * debug messages.
 * 
 * Examples:
 * 
 *     logger = require("logger")
 *     logger.debug("text to output")
 */
static int logger_debug(lua_State *L) {
  const char *str = lua_tostring(L, 1);
  LDEBUG("lua: %s", str);
  return 0;
}

/*
 * Log output at the INFO level. No effect if the current log level restricts
 * info messages.
 * 
 * Examples:
 * 
 *     logger = require("logger")
 *     logger.info("text to output")
 */
static int logger_info(lua_State *L) {
  const char *str = lua_tostring(L, 1);
  LINFO("lua: %s", str);
  return 0;
}

/*
 * Log output at the WARN level. No effect if the current log level restricts
 * warning messages.
 * 
 * Examples:
 * 
 *     logger = require("logger")
 *     logger.warn("text to output")
 */
static int logger_warn(lua_State *L) {
  const char *str = lua_tostring(L, 1);
  LWARN("lua: %s", str);
  return 0;
}

/*
 * Log output at the error level. No effect if the current log level restricts
 * error messages.
 * 
 * Examples:
 * 
 *     logger = require("logger")
 *     logger.error("text to output")
 */
static int logger_error(lua_State *L) {
  const char *str = lua_tostring(L, 1);
  LERROR("lua: %s", str);
  return 0;
}

/*
 * Assign the log level. Messages lower than the specified log level will be
 * silenced.
 * 
 * Examples:
 * 
 *     logger = require("logger")
 *     logger.level(logger.INSECURE)  -- silence no messages
 *     logger.level(logger.TRACE)     -- silence INSECURE messages
 *     logger.level(logger.DEBUG)     -- silence TRACE and INSECURE messages
 *     logger.level(logger.INFO)      -- silence DEBUG, TRACE and INSECURE messages
 *     logger.level(logger.WARN)      -- silence INFO, DEBUG, TRACE and INSECURE messages
 *     logger.level(logger.ERROR)     -- silence WARN, INFO, DEBUG, TRACE and INSECURE messages
 *     logger.level(logger.SILENT)    -- silence all messages
 */
static int logger_level(lua_State *L) {
  if (lua_gettop(L) > 0) {
    int level = luaL_checkinteger(L, 1);
    LSETLEVEL(level);
  }

  lua_pushinteger(L, LGETLEVEL());
  return 1;
}

static const luaL_Reg logger_methods[] = {
  {"insecure", logger_insecure},
  {"trace",    logger_trace},
  {"debug",    logger_debug},
  {"info",     logger_info},
  {"warn",     logger_warn},
  {"error",    logger_error},
  {"level",    logger_level},
  {NULL,       NULL}
};

LUALIB_API int luaopen_logger(lua_State *L) {
  lua_newtable(L);
  lua_pushinteger(L, LOG_LEVEL_INSEC);
  lua_setfield(L, -2, "INSECURE");
  lua_pushinteger(L, LOG_LEVEL_TRACE);
  lua_setfield(L, -2, "TRACE");
  lua_pushinteger(L, LOG_LEVEL_DEBUG);
  lua_setfield(L, -2, "DEBUG");
  lua_pushinteger(L, LOG_LEVEL_INFO);
  lua_setfield(L, -2, "INFO");
  lua_pushinteger(L, LOG_LEVEL_WARN);
  lua_setfield(L, -2, "WARN");
  lua_pushinteger(L, LOG_LEVEL_ERROR);
  lua_setfield(L, -2, "ERROR");
  lua_pushinteger(L, LOG_LEVEL_SILENT);
  lua_setfield(L, -2, "SILENT");
  luaL_setfuncs(L, logger_methods, 0);
  return 1;
}

int init_logger_lua(lua_State *L) {
  // Get package.preload so we can store builtins in it.
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_remove(L, -2); // Remove package
  lua_pushcfunction(L, luaopen_logger);
  lua_setfield(L, -2, "logger");
  return 0;
}

void shutdown_logger_lua(lua_State *L) {
  (void) L;
}
