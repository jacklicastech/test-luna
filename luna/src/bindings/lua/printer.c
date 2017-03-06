#define LUA_LIB

#include "config.h"
#include <lua.h>
#include <lauxlib.h>

#include <czmq.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "services/logger.h"
#include "util/files.h"

#ifdef HAVE_CTOS
#include <ctosapi.h>
#endif

/*
  FIXME perform error checking, raise errors when printing not supported, etc
*/

/*
 * Scrolls the printer by the specified number of dots. This results in white
 * space between the previous printed output and the next printed output.
 * 
 * Examples:
 * 
 *     printer = require("printer")
 *     printer.scroll(36)
 */
static int printer_scroll(lua_State *L) {
  int dots = lua_tonumber(L, 1);
  (void) dots;
  
#ifdef HAVE_CTOS
  CTOS_PrinterFline((unsigned short) dots);
#endif
  return 0;
}

/*
 * Prints the specified text to the printer.
 * 
 * Examples:
 * 
 *     printer = require("printer")
 *     printer.text("Hello, world!\n")
 */
static int printer_text(lua_State *L) {
  const char *str = lua_tostring(L, 1);
  (void) str;

#ifdef HAVE_CTOS
  CTOS_PrinterPutString((unsigned char *) str);
#endif
  return 0;
}

/*
 * Prints the image at the specified filename to the printer with the
 * specified horizontal offset (in dots).
 * 
 * Examples:
 * 
 *     printer = require("printer")
 *     printer.image(100, "logo.bmp")
 */
static int printer_image(lua_State *L) {
  int x = lua_tonumber(L, 1);
  char *filename = find_readable_file(NULL, lua_tostring(L, 2));
  (void) x;

  if (filename) {
#ifdef HAVE_CTOS
    CTOS_PrinterBMPPic((unsigned short) x, (unsigned char *) filename);
#endif
    free(filename);
  }
  return 0;
}


static const luaL_Reg printer_methods[] = {
  {"scroll", printer_scroll},
  {"text",   printer_text},
  {"image",  printer_image},
  {NULL,     NULL}
};

LUALIB_API int luaopen_printer(lua_State *L)
{
  lua_newtable(L);
  luaL_setfuncs(L, printer_methods, 0);

  return 1;
}

int init_printer_lua(lua_State *L) {
  // Get package.preload so we can store builtins in it.
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_remove(L, -2); // Remove package
  lua_pushcfunction(L, luaopen_printer);
  lua_setfield(L, -2, "printer");
  return 0;
}

void shutdown_printer_lua(lua_State *L) {
  (void) L;
}
