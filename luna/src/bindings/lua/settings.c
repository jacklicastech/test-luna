#define LUA_LIB

#include <lua.h>
#include <lauxlib.h>

#include <czmq.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "services/logger.h"
#include "services/settings.h"

static zsock_t *settings = NULL;

/*
 * Sets the value of one or more settings.
 * 
 * Examples:
 * 
 *     settings = require("settings")
 *     
 *     settings.set("wifi.ssid", "Dagobah")
 *     
 *     settings.set({["wifi.ssid"]     = "Dagobah",
 *                   ["wifi.password"] = "12345678"})
 */
static int _settings_set(lua_State *L) {
    if (lua_istable(L, 1)) {
        lua_pushnil(L); // first key
        while (lua_next(L, 1) != 0) {
            const char *key = lua_tostring(L, -2);
            const char *val = lua_tostring(L, -1);
            settings_set(settings, 1, key, val);
            lua_pop(L, 1); // remove val, keep key for next iteration
        }
        lua_pop(L, 1); // remove key
    } else {
        const char *key = lua_tostring(L, 1);
        const char *val = lua_tostring(L, 2);

        if (!key) {
            zsock_destroy(&settings);
            luaL_error(L, "argument must be a table or key and value strings");
        }

        settings_set(settings, 1, key, val);
    }

    return 0;
}

/*
 * Gets the value of one or more settings.
 * 
 * Examples:
 * 
 *     settings = require("settings")
 *     ssid = settings.get("wifi.ssid")
 */
static int _settings_get(lua_State *L) {
    int n = lua_gettop(L), i;
    char *val = NULL;

    for (i = 1; i <= n; i++) {
        const char *key = lua_tostring(L, i);
        if (!key) {
            zsock_destroy(&settings);
            luaL_checkstring(L, i);
        }

        settings_get(settings, 1, key, &val);
        if (val) {
            lua_pushstring(L, val);
            free(val);
        } else {
            lua_pushstring(L, "");
        }
    }

    return n;
}

/*
 * Deletes the specified settings.
 * 
 * Examples:
 * 
 *     settings = require("settings")
 *     settings.del("wifi.ssid")
 */
static int _settings_del(lua_State *L) {
    int n = lua_gettop(L), i;
    //char *val = NULL;

    for (i = 1; i <= n; i++) {
        const char *key = lua_tostring(L, i);
        if (!key) {
            zsock_destroy(&settings);
            luaL_checkstring(L, i);
        }

        settings_del(settings, 1, key);
    }

    return n;
}

/*
 * Shuts down the settings service, deletes the settings database, then
 * restarts the service, re-initializing the database from scratch. This is
 * really not recommended unless you know exactly what you are doing.
 * 
 * Examples:
 * 
 *     settings = require("settings")
 *     settings.purge()
 */
static int _settings_purge(lua_State *L) {
    settings_purge(settings);
    return 0;
}



static const luaL_Reg settings_methods[] = {
    {"set",   _settings_set},
    {"get",   _settings_get},
    {"del",   _settings_del},
    {"purge", _settings_purge},
    {NULL,    NULL}
};

LUALIB_API int luaopen_settings(lua_State *L)
{
    lua_newtable(L);
    luaL_setfuncs(L, settings_methods, 0);

    return 1;
}

int init_settings_lua(lua_State *L) {
    settings = zsock_new_req(SETTINGS_ENDPOINT);

    // Get package.preload so we can store builtins in it.
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_remove(L, -2); // Remove package

    // Store CTOS module definition at preload.CTOS
    lua_pushcfunction(L, luaopen_settings);
    lua_setfield(L, -2, "settings");

    return 0;
}

void shutdown_settings_lua(lua_State *L) {
    zsock_destroy(&settings);
    settings = NULL;
    (void) L;
}
