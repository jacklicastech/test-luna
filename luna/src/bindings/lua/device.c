#define LUA_LIB

#include "config.h"
#include <lua.h>
#include <lauxlib.h>
#include <string.h>
#include <stdint.h>
#include <czmq.h>

#include "services/logger.h"
#include "util/machine_id.h"

#ifdef HAVE_CTOS
#include <ctosapi.h>
#if HAVE_LIBCACLENTRY
#include <emv_cl.h>
#endif
#endif

/*
 * Sets the specified device LEDs to on or off, while leaving the unspecified
 * LEDs in their current state.
 *
 * Examples:
 * 
 *     device = require("device")
 *     
 *     -- turn the first 2 LEDs on
 *     device.led({ [1] = true, [2] = true })
 *     
 *     -- turn the second LED off and the third LED on; leave the first LED on
 *     device.led({ [2] = false, [3] = true })
 *
 */
static int device_led(lua_State *L) {
#ifdef HAVE_CTOS
  int which = 0, onoff = 0;
  int led;

  luaL_checktype(L, 1, LUA_TTABLE);
  for (led = 0; led < 4; led++) {
    lua_pushinteger(L, led + 1);
    lua_gettable(L, 1);
    if (!lua_isnil(L, -1)) {
      which = which | (1 << led);
      if (lua_toboolean(L, -1))
        onoff = onoff | (1 << led);
    }
    lua_pop(L, 1);
  }

  LDEBUG("lua: device: set LEDs: which = %d, state = %d", which, onoff);
#if HAVE_LIBCACLENTRY
  unsigned int err = EMVCL_SetLED((BYTE) which, (BYTE) onoff);
  switch(err) {
    case d_EMVCL_NO_ERROR: break;
    case d_EMVCL_RC_FAILURE: LWARN("lua: device: internal failure while setting LED state"); break;
    case d_EMVCL_RC_INVALID_PARAM: LWARN("lua: device: invalid parameter while setting LED state"); break;
    default: LWARN("lua: device: unrecognized result while setting LED state: %x", err);
  }
#else
  LWARN("lua: device: API to set LEDs is not available");
#endif

#else
  (void) L;
#endif

  return 0;
}

/*
 * Returns a unique ID representing this specific device. The ID is unique to
 * this machine and will survive across reboots, but may not survive across OS
 * reinstalls.
 *
 * Examples:
 * 
 *     device = require("device")
 *     print(device.id())
 */
static int device_id(lua_State *L) {
  char *id = unique_machine_id();
  lua_pushstring(L, id);
  free(id);
  return 1;
}

/*
 * Plays a short tone of the specified frequency and duration. Frequency and
 * duration are both optional and default values will be used if they are
 * not specified. The frequency is specified in hertz and the duration is
 * specified in milliseconds.
 *
 * Examples:
 * 
 *     device = require("device")
 *     device.beep()
 *     device.beep(1500)
 *     device.beep(1500, 100)
 */
static int device_beep(lua_State *L) {
  int frequency = 1500, duration = 100;
  if (lua_gettop(L) > 0) frequency = (int) lua_tonumber(L, 1);
  if (lua_gettop(L) > 1) duration  = (int) lua_tonumber(L, 2);

#ifdef HAVE_CTOS
  USHORT res = CTOS_Sound((USHORT) frequency, (USHORT) duration / 10);
  if (res != d_OK) {
    const char *msg = NULL;
    switch(res) {
      case d_OK:                  msg = "no error (?)"; break;
      case d_BUZZER_INVALID_PARA: msg = "invalid parameter"; break;
      case d_BUZZER_HAL_FAULT:    msg = "HAL fault"; break;
      default:                    msg = "unknown"; break;
    }
    LWARN("lua: device: error while playing sound: %x (invalid parameter)", res);
  }
#else
  (void) frequency;
  (void) duration;
  LINFO("lua: device: beep not supported or not implemented");
#endif

  return 0;
}

static const luaL_Reg device_methods[] = {
  {"id",   device_id},
  {"beep", device_beep},
  {"led",  device_led},
  {NULL,   NULL}
};

LUALIB_API int luaopen_device(lua_State *L) {
  lua_newtable(L);
  luaL_setfuncs(L, device_methods, 0);
  return 1;
}

int init_device_lua(lua_State *L) {
  // Get package.preload so we can store builtins in it.
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_remove(L, -2); // Remove package
  lua_pushcfunction(L, luaopen_device);
  lua_setfield(L, -2, "device");
  return 0;
}

void shutdown_device_lua(lua_State *L) {
  (void) L;
}
