#define LUA_LIB
#include "config.h"
#include <stdio.h>
#include "services.h"
#include "lua.h"
#include "lauxlib.h"
#if HAVE_CTOS
#include <ctosapi.h>
#endif

/*
 * Set the system date and time. Returns `true` if successful, `false`
 * otherwise.
 * 
 * Examples:
 * 
 *     datetime = require('datetime')
 *     datetime.set(year, month, day, hour, minute, second)
 *
 */
int datetime_set(lua_State *L) {
#if HAVE_CTOS
  CTOS_RTC rtc;
  if (CTOS_RTCGet(&rtc) != d_OK) {
    lua_pushboolean(L, 0);
    return 1;
  }

  rtc.bYear   = lua_tonumber(L, 1);
  rtc.bMonth  = lua_tonumber(L, 2);
  rtc.bDay    = lua_tonumber(L, 3);
  rtc.bHour   = lua_tonumber(L, 4);
  rtc.bMinute = lua_tonumber(L, 5);
  rtc.bSecond = lua_tonumber(L, 6);

  if (CTOS_RTCSet(&rtc) != d_OK) {
    lua_pushboolean(L, 1);
    return 1;
  }
#endif

  lua_pushboolean(L, 0);
  return 1;
}

/*
 * Get the system date and time. There are 6 return values: year, month, day,
 * hour, minute and second. If an error occurs all values will be -1.
 * 
 * Examples:
 * 
 *     datetime = require('datetime')
 *     year, month, day, hour, minute, second = datetime.get()
 *
 */
int datetime_get(lua_State *L) {
#if HAVE_CTOS
  CTOS_RTC rtc;
  if (CTOS_RTCGet(&rtc) == d_OK) {
    lua_pushnumber(L, rtc.bYear);
    lua_pushnumber(L, rtc.bMonth);
    lua_pushnumber(L, rtc.bDay);
    lua_pushnumber(L, rtc.bHour);
    lua_pushnumber(L, rtc.bMinute);
    lua_pushnumber(L, rtc.bSecond);
    return 6;
  }
#endif
  lua_pushnumber(L, -1);
  lua_pushnumber(L, -1);
  lua_pushnumber(L, -1);
  lua_pushnumber(L, -1);
  lua_pushnumber(L, -1);
  lua_pushnumber(L, -1);
  return 6;
}

static const luaL_Reg datetime_methods[] = {
  {"get", datetime_get},
  {"set", datetime_set},
  {NULL,  NULL}
};

LUALIB_API int luaopen_datetime(lua_State *L) {
  lua_newtable(L);
  luaL_setfuncs(L, datetime_methods, 0);
  return 1;
}

void shutdown_datetime_lua(lua_State *L) {
  (void) L;
}
