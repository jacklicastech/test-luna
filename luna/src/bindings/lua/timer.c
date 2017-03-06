#define LUA_LIB

#include "config.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdint.h>
#include <czmq.h>

#include "services/logger.h"
#include "services/timer.h"

static zsock_t *timer_socket = NULL;

/*
 * Create a new timer. Returns the timer ID, which will be the name of the
 * topic that will be broadcast on when the timer has expired. Takes the
 * number of milliseconds before the timer expires as an argument.
 *
 * Intended for use in combination with events.
 * 
 * Examples:
 * 
 *     timer = require("timer")
 *     events = require("events")
 *     id = timer.new(100)
 *     events.focus({trigger = function() ... end}, "timer " .. id)
 */
static int timer_new(lua_State *L) {
  char *id;
  zsock_send(timer_socket, "i", (int) luaL_checknumber(L, 1));
  zsock_recv(timer_socket, "s", &id);
  lua_pushstring(L, id);
  LDEBUG("lua: created timer: %s", id);
  free(id);
  return 1;
}

static const luaL_Reg timer_methods[] = {
  {"new", timer_new},
  {NULL,  NULL}
};

LUALIB_API int luaopen_timer(lua_State *L) {
  lua_newtable(L);
  luaL_setfuncs(L, timer_methods, 0);
  return 1;
}

int init_timer_lua(lua_State *L) {
  if (!timer_socket) timer_socket = zsock_new_req(TIMER_REQUEST);

  // Get package.preload so we can store builtins in it.
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_remove(L, -2); // Remove package
  lua_pushcfunction(L, luaopen_timer);
  lua_setfield(L, -2, "timer");
  return 0;
}

void shutdown_timer_lua(lua_State *L) {
  (void) L;
  if (timer_socket) zsock_destroy(&timer_socket);
  timer_socket = NULL;
}
