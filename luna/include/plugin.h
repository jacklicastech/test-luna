#ifndef PLUGIN_H
#define PLUGIN_H

#include "config.h"
#define _GNU_SOURCE

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "services.h"

typedef struct {
  struct {
    int  (*init)(int argc, char *argv[]);
    bool (*is_running)(void);
    void (*shutdown)(void);
  } service;

  struct {
    int (*init)(lua_State *L);
    void (*shutdown)(void);
  } binding;

  char name[PATH_MAX];
  void *handle;
  UT_hash_handle hh;
} plugin_t;

plugin_t *find_plugin_by_name(const char *name);

#endif // PLUGIN_H
