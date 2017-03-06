/*
 * It's no longer necessary to define SINGLE_THREADED_LOGGER. All logging is now
 * performed on the thread that initiated it. If this becomes an issue the
 * commit can be reverted.
 */

#include "config.h"
#define _GNU_SOURCE
#include "io/signals.h"
#include "services/logger.h"
#include "services/settings.h"
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <czmq.h>

typedef struct {
  struct timeval time;
  int level;
  char *data;
} logmessage;

static int log_level;
//FILE *out = stdout;

//static zactor_t *logger;

static void write_timestamp(struct timeval time) {
  time_t nowtime;
  struct tm *nowtm;
  char buf[64];
  
  nowtime = time.tv_sec;
  nowtm = localtime(&nowtime);
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", nowtm);
  printf("%s", buf);
}

static void write_logmsg(struct timeval time, int level, const char *data) {
  #define Blue    "\x1b[34m"
  #define Green   "\x1b[32m"
  #define Yellow  "\x1b[33m"
  #define Red     "\x1b[31m"
  #define Regular "\x1b[0m"
  #define Magenta "\x1b[38;5;013m"

  switch(level) {
    case LOG_LEVEL_DEBUG: printf(Blue);   break;
    case LOG_LEVEL_INFO:  printf(Green);  break;
    case LOG_LEVEL_WARN:  printf(Yellow); break;
    case LOG_LEVEL_ERROR: printf(Red);    break;
    case LOG_LEVEL_INSEC: printf(Magenta);    break;
    default: break;
  }

  write_timestamp(time);
  printf(" ");
  switch(level) {
    case LOG_LEVEL_TRACE: printf("T "); break;
    case LOG_LEVEL_INSEC: printf("S "); break;
    case LOG_LEVEL_ERROR: printf("E "); break;
    case LOG_LEVEL_WARN:  printf("W "); break;
    case LOG_LEVEL_INFO:  printf("I "); break;
    case LOG_LEVEL_DEBUG: printf("D "); break;
    default:              printf("U ");
  }
  printf("%s" Regular "\n", data);
}

static char *logmsg_format(const char *fmt, va_list args) {
  char *data = NULL;
  if (vasprintf(&data, fmt, args)) {}
  return data;
}

static void logmsg(int level, char *data) {
  struct timeval time;
  gettimeofday(&time, NULL);
  write_logmsg(time, level, data);
  free(data);
}

void LINFO(const char *fmt, ...) {
  va_list args;
  if (LOG_LEVEL_INFO >= log_level) {
    va_start(args, fmt);
    logmsg(LOG_LEVEL_INFO, logmsg_format(fmt, args));
    va_end(args);
  }
}

void LTRACE(const char *fmt, ...) {
  va_list args;
  if (LOG_LEVEL_TRACE >= log_level) {
    va_start(args, fmt);
    logmsg(LOG_LEVEL_TRACE, logmsg_format(fmt, args));
    va_end(args);
  }
}

void LINSEC(const char *fmt, ...) {
#if LOG_INSECURE_MESSAGES
  va_list args;
  if (LOG_LEVEL_TRACE >= log_level) {
    va_start(args, fmt);
    logmsg(LOG_LEVEL_INSEC, logmsg_format(fmt, args));
    va_end(args);
  }
#endif
}

void LDEBUG(const char *fmt, ...) {
  va_list args;
  if (LOG_LEVEL_DEBUG >= log_level) {
    va_start(args, fmt);
    logmsg(LOG_LEVEL_DEBUG, logmsg_format(fmt, args));
    va_end(args);
  }
}

void LWARN(const char *fmt, ...) {
  va_list args;
  if (LOG_LEVEL_WARN >= log_level) {
    va_start(args, fmt);
    logmsg(LOG_LEVEL_WARN, logmsg_format(fmt, args));
    va_end(args);
  }
}

void LERROR(const char *fmt, ...) {
  va_list args;
  if (LOG_LEVEL_ERROR >= log_level) {
    va_start(args, fmt);
    logmsg(LOG_LEVEL_ERROR, logmsg_format(fmt, args));
    va_end(args);
  }
}

void LSETLEVEL(int level) {
  char ch[5];
  zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);

  switch(level) {
    case LOG_LEVEL_INSEC:  LINFO("logger: setting level to INSECURE"); break;
    case LOG_LEVEL_TRACE:  LINFO("logger: setting level to TRACE");    break;
    case LOG_LEVEL_DEBUG:  LINFO("logger: setting level to DEBUG");    break;
    case LOG_LEVEL_INFO:   LINFO("logger: setting level to INFO");     break;
    case LOG_LEVEL_WARN:   LINFO("logger: setting level to WARN");     break;
    case LOG_LEVEL_ERROR:  LINFO("logger: setting level to ERROR");    break;
    case LOG_LEVEL_SILENT: LINFO("logger: setting level to SILENT");   break;
    default:
      LWARN("logger: won't set log level to unrecognized value %d", level);
      goto lsetlevel_cleanup;
  }

  log_level = level;
  sprintf(ch, "%d", level);
  settings_set(settings, 1, "logger.level", ch);
lsetlevel_cleanup:
  zsock_destroy(&settings);
}

int LGETLEVEL() {
  return log_level;
}

int init_logger_service(int level) {
  log_level = level;
//    out = fopen("cag.log", "w");
  return 0;
}

void shutdown_logger_service(void) {
//    if (out != stdout)
//        fclose(out);
//    out = stdout;
}
