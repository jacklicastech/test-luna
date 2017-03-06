#define LUA_LIB

#include "config.h"
#include <lua.h>
#include <lauxlib.h>

#include <czmq.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "services.h"
#include "plugin.h"

#ifdef HAVE_CTOS
#include <ctosapi.h>
#endif

/*
 * Starts the named service.
 * 
 * Examples:
 * 
 *     services = require("services")
 *     services.start("keypad")
 */
static int services_start(lua_State *L) {
  const char *svc_name = luaL_checkstring(L, 1);
  // if (!strcmp(svc_name, "keypad")) init_keypad_service();
  // else {
    plugin_t *plugin = find_plugin_by_name(svc_name);
    if (plugin) {
      if (plugin->service.init) {
        plugin->service.init(0, NULL);
      } else {
        LWARN("services: plugin %s is not a service", svc_name);
      }
    } else {
      LWARN("services: service %s not found", svc_name);
    }
  // }
  return 0;
}

/*
 * Stops the named service.
 * 
 * Examples:
 * 
 *     services = require("services")
 *     services.stop("keypad")
 */
static int services_stop(lua_State *L) {
  const char *svc_name = luaL_checkstring(L, 1);
  // if (!strcmp(svc_name, "keypad")) shutdown_keypad_service();
  // else {
    plugin_t *plugin = find_plugin_by_name(svc_name);
    if (plugin) {
      if (plugin->service.shutdown) {
        plugin->service.shutdown();
      } else {
        LWARN("services: plugin %s can not be shut down", svc_name);
      }
    } else {
      LWARN("services: service %s not found", svc_name);
    }
  // }
  return 0;
}

static const luaL_Reg services_methods[] = {
  {"start", services_start},
  {"stop",  services_stop},
  {NULL,     NULL}
};

LUALIB_API int luaopen_services(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, services_methods, 0);
  return 1;
}

int init_services_lua(lua_State *L) {
  // Get package.preload so we can store builtins in it.
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_remove(L, -2); // Remove package
  lua_pushcfunction(L, luaopen_services);
  lua_setfield(L, -2, "services");
  return 0;
}

void shutdown_services_lua(lua_State *L) {
  (void) L;
}
