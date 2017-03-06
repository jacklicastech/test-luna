#include <stdio.h>
#include <execinfo.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "services.h"
#include "util/headers_parser.h"
#include "util/uthash.h"

void crash_handler(int sig) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

int main(int argc, char **argv) {
  header_t *headers = NULL;
  header_t *header = NULL;
  const char *req = "Content-Type: application/json\r\nAuthorization: Basic dXNlcitwYXNzd2Q6\r\n\r\nBODY";
  int ofs;

  init_logger_service(LOG_LEVEL_DEBUG);

  ofs = parse_headers(&headers, req, strlen(req));
  assert(!strcmp(req + ofs, "BODY"));

  HASH_FIND_STR(headers, "content-type", header);
  assert(header);
  assert(!strcmp(header->value, "application/json"));

  HASH_FIND_STR(headers, "authorization", header);
  assert(header);
  assert(!strcmp(header->value, "Basic dXNlcitwYXNzd2Q6"));

  free_headers(&headers);

  shutdown_logger_service();
  return 0;
}
