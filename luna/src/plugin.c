#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <dlfcn.h>

#include "plugin.h"
#include "services.h"
#include "util/uthash.h"
#include "util/files.h"

#define ERR_OK                         0
#define ERR_PLUGIN_FILE_UNLOADABLE    -1
#define ERR_PLUGIN_DUPLICATE_NAME     -2
#define ERR_PLUGIN_FUNCTION_NOT_FOUND -3
#define ERR_FS_DIR_CANNOT_OPEN        -4

static plugin_t *plugins = NULL;

static void *find_plugin_function(plugin_t *plugin,
                                const char *prefix,
                                const char *suffix) {
  char tmp[PATH_MAX];
  if (suffix != NULL)
    sprintf(tmp, "%s_%s_%s", prefix, plugin->name, suffix);
  else
    sprintf(tmp, "%s_%s", prefix, plugin->name);
  void *out = dlsym(plugin->handle, tmp);
  if (!out) {
    LDEBUG("%s: has no function '%s'", plugin->name, tmp);
    return NULL;
  }

  return out;
}

static int init_plugin_file(const arguments_t *arguments,
                            const char *path, const char *basename,
                            int argc, char *argv[]) {
  int err = ERR_OK;
  int i;

  plugin_t *plugin = malloc(sizeof(plugin_t));
  memset(plugin, 0, sizeof(plugin_t));
  if (!strncmp(basename, "lib", 3)) basename += 3;
  assert(strlen(basename) >= 3);
  memcpy(plugin->name, basename, strlen(basename) - 3); // '.so' == 3
  if (arguments != NULL) {
    for (i = 0; i < arguments->num_disabled_plugins; i++) {
      if (!strcmp(plugin->name, arguments->disabled_plugins[i])) {
        LINFO("plugin: %s: skipping plugin at %s", plugin->name, path);
        goto plugin_load_failed;
      }
    }
  }

  LINFO("plugin: %s: loading plugin at %s", plugin->name, path);
  plugin_t *existing = NULL;
  HASH_FIND_STR(plugins, plugin->name, existing);
  if (existing != NULL) {
    LERROR("plugin: %s: plugin with same name was already loaded", plugin->name);
    err = ERR_PLUGIN_DUPLICATE_NAME;
    goto plugin_load_failed;
  }

  plugin->handle = dlopen(path, RTLD_NOW);
  if (!(plugin->handle)) {
    LERROR("plugin: %s: could not load object file: %s", plugin->name, dlerror());
    err = ERR_PLUGIN_FILE_UNLOADABLE;
    goto plugin_load_failed;
  }

  void *funcptr;
  if ((funcptr = find_plugin_function(plugin, "init",     "service")))
    memcpy(&plugin->service.init,       &funcptr, sizeof(plugin->service.init));
  if ((funcptr = find_plugin_function(plugin, "shutdown", "service")))
    memcpy(&plugin->service.shutdown,   &funcptr, sizeof(plugin->service.shutdown));
  if ((funcptr = find_plugin_function(plugin, "is",       "service_running")))
    memcpy(&plugin->service.is_running, &funcptr, sizeof(plugin->service.is_running));
  if ((funcptr = find_plugin_function(plugin, "luaopen",  NULL)))
    memcpy(&plugin->binding.init,       &funcptr, sizeof(plugin->binding.init));
  if ((funcptr = find_plugin_function(plugin, "shutdown", "lua")))
    memcpy(&plugin->binding.shutdown,   &funcptr, sizeof(plugin->binding.shutdown));

  if (plugin->service.init) {
    if ((err = plugin->service.init(argc, argv))) {
      LERROR("plugin: %s: failed to initialize: %d", plugin->name, err);
      goto plugin_load_failed;
    }
  }

  HASH_ADD_KEYPTR(hh, plugins, plugin->name, strlen(plugin->name), plugin);
  return ERR_OK;

plugin_load_failed:
  if (plugin->handle)
    dlclose(plugin->handle);
  free(plugin);
  return err;
}

static void shutdown_plugin(plugin_t *plugin) {
  LDEBUG("plugin: %s: sending shutdown signal", plugin->name);
  if (plugin->service.shutdown) plugin->service.shutdown();
  dlclose(plugin->handle);
  free(plugin);
}

static int recursively_find_and_load_plugins(const arguments_t *arguments,
                                             const char *dir_name,
                                             int argc, char *argv[]) {
  DIR *d;
  d = opendir(dir_name);
  if (!d) {
    LERROR("Could not open directory: %s", strerror(errno));
    return ERR_FS_DIR_CANNOT_OPEN;
  }

  while (1) {
    struct dirent *entry;
    const char *d_name;
    int path_length;
    char path[PATH_MAX];
    int err;

    entry = readdir(d);
    if (!entry) break;

    d_name = entry->d_name;
    path_length = snprintf(path, PATH_MAX, "%s/%s", dir_name, d_name);
    if (path_length < PATH_MAX) {
      if (!(entry->d_type & DT_DIR)) {
        // found non-directory
        char *dot = strrchr(path, '.');
        if (!strcmp(dot, ".so")) {
          // found shared object file
          if ((err = init_plugin_file(arguments, path, d_name, argc, argv))) {
            LERROR("Failed to load plugin file: %s", path);
            LDEBUG("Treating as nonfatal. ");
            // try to treat error as nonfatal
            // closedir(d);
            // return err;
          }
        }
      } else {
        if (strcmp(d_name, "..") != 0 &&
            strcmp(d_name, ".")  != 0) {
          recursively_find_and_load_plugins(arguments, path, argc, argv);
        }
      }
    } else {
      LWARN("Path length is too long, not evaluating: %s/%s",
           dir_name, d_name);
    }
  }

  closedir(d);
  return ERR_OK;
}

int init_plugin_lua_bindings(lua_State *L) {
  lua_getglobal(L, "package");
  lua_getfield(L, -1, "preload");
  lua_remove(L, -2); // Remove package

  plugin_t *plugin, *tmp;
  HASH_ITER(hh, plugins, plugin, tmp) {
    if (plugin->binding.init) {
      LINFO("plugin: %s: adding lua binding", plugin->name);
      lua_getglobal(L, "package");
      lua_getfield(L, -1, "preload");
      lua_remove(L, -2); // Remove package
      lua_pushcfunction(L, plugin->binding.init);
      lua_setfield(L, -2, plugin->name);
    } else {
      LDEBUG("plugin: %s: has no lua binding", plugin->name);
    }
  }

  return 0;
}

void shutdown_plugin_lua_bindings(lua_State *L) {
  plugin_t *plugin, *tmp;
  HASH_ITER(hh, plugins, plugin, tmp) {
    if (plugin->binding.shutdown) {
      LINFO("plugin: %s: shutting down lua binding", plugin->name);
      plugin->binding.shutdown();
    } else {
      LDEBUG("plugin: %s: has no shutdown for lua binding", plugin->name);
    }
  }
}

plugin_t *find_plugin_by_name(const char *name) {
  plugin_t *plugin, *tmp;
  HASH_ITER(hh, plugins, plugin, tmp) {
    if (!strcmp(plugin->name, name))
      return plugin;
  }
  return NULL;

  // plugin_t *plugin = NULL;
  // HASH_FIND_STR(plugins, name, plugin);
  // return plugin;
}

int init_plugins(const arguments_t *arguments, int argc, char *argv[]) {
  const char *plugins_path = getenv("PLUGINS_PATH");
  if (plugins_path == NULL) plugins_path = "plugins";
  char *search_path = find_readable_file(NULL, plugins_path);
  assert(search_path != NULL);
  int err = recursively_find_and_load_plugins(arguments, search_path, argc, argv);
  free(search_path);
  if (err) return err;
  return ERR_OK;
}

void shutdown_plugins(void) {
  plugin_t *plugin, *tmp;
  HASH_ITER(hh, plugins, plugin, tmp) {
    HASH_DEL(plugins, plugin);
    shutdown_plugin(plugin);
  }
}
