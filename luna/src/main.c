#define _GNU_SOURCE
#include "config.h"
#include <curl/curl.h>
#include <execinfo.h>
#include <libxml/xpath.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#if HAVE_LIBBACKTRACE
  #include <backtrace.h>
  #include <backtrace-supported.h>
#endif

#include "czmq.h"
#include "services.h"
#include "ssl_locks.h"
#include "bindings.h"
#include "cli.h"
#include "util/files.h"
#include "util/encryption_helpers.h"

#ifdef HAVE_CTOS
#include <ctosapi.h>
#endif

bool ALLOW_DISABLE_SSL_VERIFICATION = false;
unsigned short app_index = 0;
char cacerts_bundle[PATH_MAX];

#if HAVE_LIBBACKTRACE
  static void *bt_state;
#endif

static void splash(const char *appName) {
#ifdef HAVE_CTOS
  // Get the application index
  char *buf = find_readable_file(NULL, "screen-castles-splash.bmp");
  // Set the display to GRAPHIC mode
  CTOS_LCDSelectMode(d_LCD_GRAPHIC_MODE); 
  CTOS_LCDTTFSelect((unsigned char *) "arial.ttf", 0);
  CTOS_LCDFontSelectMode(d_FONT_TTF_MODE);
  CTOS_LCDGShowBMPPic(0, 0, (BYTE *) buf);
  free(buf);
#endif
  LINFO(PACKAGE_NAME " v" PACKAGE_VERSION);
}

static void clean_shutdown(int power_off) {
  LDEBUG("Shutting down");
  shutdown_plugins();
  shutdown_touchscreen_service();
  shutdown_input_service();
  shutdown_webserver_service();
  shutdown_usb_service();
  shutdown_bluetooth_service();
  // shutdown_autoupdate_service();
  shutdown_wifi_service();
  shutdown_settings_service();
  shutdown_tokenizer_service();
  shutdown_timer_service();
  shutdown_events_proxy_service();
  shutdown_logger_service();

  xmlCleanupParser();
  curl_global_cleanup();
  shutdown_ssl_locks();
  zsys_shutdown();

#ifdef HAVE_CTOS
  if (power_off)
    CTOS_PowerMode(d_PWR_POWER_OFF);
#endif
}

// HACK when installing program updates, CTOS attempts to restart (?) the
// application, but gives no time for a graceful termination, resulting in
// zmq assertion failures and dangling sockets. This function catches the
// assertion failures and "handles" them with a system reboot. It's fugly but
// the end user probably won't notice a difference.
static void termination_handler(int signum) {
  LERROR("main: received signal %d", signum);

#if HAVE_LIBBACKTRACE
  backtrace_print(bt_state, 5, stderr);
#else
  void *array[20];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 20);

  // print out all the frames to stderr
  backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif

#if HAVE_CTOS
  CTOS_PowerMode(d_PWR_REBOOT);
#endif

  exit(1);
}

// static void sigint(int signum) {
//   clean_shutdown(0);
//   exit(0);
// }

/*
 * Given a filename, returns the full path to the file it represents relative
 * to paths in READ_PATHS if it exists, or NULL if it doesn't exist.
 */
static char *script_exist(const char *filename) {
  char *filepath = find_readable_file(NULL, filename);
  if (filepath == NULL) {
    LDEBUG("main: script candidate rejected: %s", filename);
    return NULL;
  }
  return filepath;
}

static int execute_scripts(arguments_t *arguments) {
  int i, err = 0;
  char *filename = NULL;

  if (arguments->num_scripts > 0) {
    for (i = 0; i < arguments->num_scripts; i++) {
      script_t *script = &(arguments->scripts[i]);

      // executor knows that a NULL filename means use stdin
      if (!strcmp(script->file, "-")) {
        free(script->file);
        script->file = NULL;
      }

      if (script->execute) {
        err = script->execute(script->file);
      } else {
        LERROR("cli: BUG: no script executor for %s", script->file);
        err = -1;
      }

      if (err) break;
    }
  } else {
    // No command line scripts specified, so scan for default script files

    // try lua
    if ((filename = script_exist("main.lua"))) { err = lua_run_file(filename); }
    else { LERROR("main: no script file to execute"); }
    if (filename) free(filename);
  }

  // cleanup
  for (i = 0; i < arguments->num_scripts; i++)
    free(arguments->scripts[i].file);
  free(arguments->scripts);

  return err;
}

#if HAVE_LIBBACKTRACE
  static void error_callback_bt_create(void *data ATTRIBUTE_UNUSED, const char *msg,
                                       int errnum) {
    fprintf(stderr, "%s", msg);
    if (errnum > 0)
      fprintf(stderr, ": %s", strerror (errnum));
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
  }
#endif

int main(int argc, char *argv[]) {
  int err, i;
  int shutdown = 0;
  char *cacerts = NULL;
  char *log_level = NULL;
  zsock_t *settings = NULL;

#if HAVE_LIBBACKTRACE
  bt_state = backtrace_create_state(argv[0], BACKTRACE_SUPPORTS_THREADS,
                                             error_callback_bt_create, NULL);
#endif

  arguments_t arguments;
  memset(&arguments, 0, sizeof(arguments));
  arguments.flags = CLI_SERVICE_ALL;

  zsys_handler_set(NULL);
  if (signal(SIGABRT, termination_handler) == SIG_IGN) signal(SIGABRT, SIG_IGN);
  if (signal(SIGSEGV, termination_handler) == SIG_IGN) signal(SIGSEGV, SIG_IGN);
  // if (signal(SIGINT,  sigint)              == SIG_IGN) signal(SIGINT,  SIG_IGN);

  // Ignore SIGPIPE errors. This allows sockets to return EPIPE if the
  // connection is reset while reading, instead of just ending the program.
  // https://curl.haxx.se/mail/lib-2008-01/0194.html
  // http://stackoverflow.com/questions/32040760/c-openssl-sigpipe-when-writing-in-closed-pipe
  // http://stackoverflow.com/questions/108183/how-to-prevent-sigpipes-or-handle-them-properly
  signal(SIGPIPE, SIG_IGN);

  splash(argv[0]);

#ifdef CAG_LOG_LEVEL
  if ((err = init_logger_service(CAG_LOG_LEVEL))) return err;
#else
  if ((err = init_logger_service(LOG_LEVEL_DEBUG))) return err;
#endif

  if ((err = init_events_proxy_service())) return err;

  LINFO("READ_PATHS : %s", getenv("READ_PATHS")  == NULL ? DEFAULT_READ_PATHS  : getenv("READ_PATHS"));
  LINFO("WRITE_PATHS: %s", getenv("WRITE_PATHS") == NULL ? DEFAULT_WRITE_PATHS : getenv("WRITE_PATHS"));

  cacerts = find_readable_file(NULL, "cacerts.pem");
  if (!cacerts) {
    LWARN("CA certs bundle could not be loaded: backend connections will fail");
    cacerts_bundle[0] = '\0';
  } else {
    sprintf(cacerts_bundle, "%s", cacerts);
    free(cacerts);
  }

  if (init_encryption()) {
    LERROR("FATAL: main: failed to initialize encryption");
    goto shutdown;
  }
  
  LDEBUG("argc: %d", argc);
  for (i = 0; i < argc; i++) {
    LDEBUG("  arg %d: %s", i, argv[i]);
  }

  if (cli_parse_options(&arguments, argc, argv))
    return 1;

  init_ssl_locks();
  curl_global_init(CURL_GLOBAL_ALL);
  xmlInitParser();

  if (arguments.flags & CLI_SERVICE_SETTINGS        && (err = init_settings_service()))        goto shutdown;

  // because we must initialize the logger before initializing settings, we
  // must load the log level from settings afterward.
  settings = zsock_new_req(SETTINGS_ENDPOINT);
  assert(!settings_get(settings, 1, "logger.level", &log_level));
  zsock_destroy(&settings);
  LSETLEVEL(atoi(log_level));
  free(log_level);

  if (arguments.flags & CLI_SERVICE_TIMER           && (err = init_timer_service()))           goto shutdown;
  if (arguments.flags & CLI_SERVICE_TOKENIZER       && (err = init_tokenizer_service()))       goto shutdown;
  if (arguments.flags & CLI_SERVICE_WIFI            && (err = init_wifi_service()))            goto shutdown;
  if (arguments.flags & CLI_SERVICE_USB             && (err = init_usb_service()))             goto shutdown;
  if (arguments.flags & CLI_SERVICE_BLUETOOTH       && (err = init_bluetooth_service()))       goto shutdown;
  if (arguments.flags & CLI_SERVICE_WEBSERVER       && (err = init_webserver_service()))       goto shutdown;
  if (arguments.flags & CLI_SERVICE_INPUT           && (err = init_input_service()))           goto shutdown;
  if (arguments.flags & CLI_SERVICE_TOUCHSCREEN     && (err = init_touchscreen_service()))     goto shutdown;

  if ((err = init_plugins(&arguments, argc, argv))) goto shutdown;
  err = execute_scripts(&arguments);

shutdown:
  clean_shutdown(shutdown);
  return err;
}
