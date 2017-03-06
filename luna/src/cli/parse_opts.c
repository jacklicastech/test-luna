#include "config.h"
#include <unistd.h>
#include <argp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "cli.h"
#include "bindings.h"
#include "services/logger.h"

#define LUA 1

static struct argp_option options[] = {
  {"exec-lua",           'l', "FILE", 0, "Run specified file, or stdin if FILE == '-', as lua", 0},
  {"disable",            'd', "NAME", 0, "Disable the named plugin or service",                 0},
  
  {"stub1",              'q',          0, 0, "no effect, present as workaround for present firmware", 2},
  {"stub2",              'w',          0, 0, "no effect, present as workaround for present firmware", 2},
  {"stub3",              's',          0, 0, "no effect, present as workaround for present firmware", 2},
  { 0 }
};

// List of all services and corresponding flags which are part of core luna,
// and are not implemented as plugins.
static struct { const char *name; int flag; } core_services[] = {
  { "tokenizer",   CLI_SERVICE_TOKENIZER   },
  { "settings",    CLI_SERVICE_SETTINGS    },
  { "wifi",        CLI_SERVICE_WIFI        },
  { "autoupdate",  CLI_SERVICE_AUTOUPDATE  },
  { "usb",         CLI_SERVICE_USB         },
  { "bluetooth",   CLI_SERVICE_BLUETOOTH   },
  { "webserver",   CLI_SERVICE_WEBSERVER   },
  { "input",       CLI_SERVICE_INPUT       },
  { "timer",       CLI_SERVICE_TIMER       },
  { "touchscreen", CLI_SERVICE_TOUCHSCREEN },
  { NULL, 0 }
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
  arguments_t *arguments = (arguments_t *) state->input;

  bool found;
  int i;

  switch (key) {
    case 'l':
      arguments->num_scripts++;
      arguments->scripts = (script_t *) realloc(arguments->scripts, arguments->num_scripts * sizeof(script_t));
      if (arg) arguments->scripts[arguments->num_scripts - 1].file = strdup(arg);
      arguments->scripts[arguments->num_scripts - 1].execute = lua_run_file;
      break;

    case 'd':
      if (!arg) {
        argp_usage(state);
        return ARGP_ERR_UNKNOWN;
      }

      found = false;
      for (i = 0; core_services[i].name != NULL; i++) {
        if (!strcmp(core_services[i].name, arg)) {
          LTRACE("opts: will not start service: %s", arg);
          arguments->flags ^= core_services[i].flag;
          found = true;
          break;
        }
      }

      if (!found) {
        LTRACE("opts: will not load plugin: %s", arg);
        arguments->disabled_plugins = realloc(arguments->disabled_plugins, ++arguments->num_disabled_plugins * sizeof(char *));
        arguments->disabled_plugins[arguments->num_disabled_plugins - 1] = strdup(arg);
      }

      break;

    case ARGP_KEY_ARG: break;
    case ARGP_KEY_END: break;
    // default:
      // don't fail if unrecognized args are given, they might be delegated to
      // dependencies
      // return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

int cli_parse_options(arguments_t *arguments, int argc, char **argv) {
  struct argp argp = {
    options,
    parse_opt,
    NULL,
    PACKAGE_NAME " " PACKAGE_VERSION " -- payment application"
  };

  return argp_parse(&argp, argc, argv, 0, 0, arguments);
  // return argp_parse(&argp, argc, argv, ARGP_NO_EXIT, 0, arguments);
}
