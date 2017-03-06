#include "config.h"
#include <stdio.h>
#include "services.h"
#ifdef HAVE_CTOS
#include <ctosapi.h>
#endif

// how many times per second to poll for user input. Higher means worse
// battery life. FIXME can we select() on a device instead?
#define FREQUENCY 10

static zactor_t *service = NULL;

/*
 * Checks for keypad/keyboard input and places the key into `*out` if input
 * is available. Returns 0 if input was available, -2 if it was not available,
 * -1 at the end of the input stream indicating input will never be available
 * again.
 */
int check_key(char *out) {
#ifdef HAVE_CTOS
  USHORT res;
  BYTE key = 0;
  res = CTOS_KBDHit(&key);
  if (res != d_KBD_INVALID_KEY && key != 0xff) {
    *out = (char) key;
    return 0;
  } else {
    return -2;
  }
#else
  fd_set fds;
  int maxfd = STDIN_FILENO + 1;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds); 
  struct timeval timeout;
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  select(maxfd+1, &fds, NULL, NULL, &timeout);
  if (FD_ISSET(STDIN_FILENO, &fds)){
    LTRACE("keypad: key press detected");
    int key = getchar();
    if (key == EOF) {
      LWARN("keypad: EOF received while reading from STDIN; service will stop");
      return -1;
    } else {
      *out = key;
      return 0;
    }
  } else {
    return -2;
  }
#endif
}

#include "util/files.h"

void keypad_service(zsock_t *pipe, void *arg) {
  zsock_t *keypad_pub = zsock_new_pub(">" EVENTS_PUB_ENDPOINT);
  zpoller_t *poller = zpoller_new(pipe, NULL);
  long wait_ms = 1000 / FREQUENCY;
  LINFO("keypad: initializing service");
  zsock_signal(pipe, 0);
  int more = 1;

  while (more) {
    void *in = zpoller_wait(poller, wait_ms);

    if (!in) {
        if (!zpoller_expired(poller)) {
            LWARN("keypad: service interrupted");
            break;
        }
    }

    if (in == pipe) {
      LDEBUG("keypad: received shutdown signal");
      break;
    }
    
    // check for keypad data
    char str[] = { 0, 0 };
    switch(check_key(str)) {
      case -1:
        // end of input, kill service
        more = 0;
        break;
      case 0:
        zsock_send(keypad_pub, "ssss", "keypad", "key-pressed", "key", str);
        break;
      default:
        // no input available at this time
        (void) 0;
    }
  }

  LINFO("keypad: terminating service");
  zsock_destroy(&keypad_pub);
  zpoller_destroy(&poller);

  LINFO("keypad: shutdown complete");
}

int init_keypad_service() {
  if (!service) service = zactor_new(keypad_service, NULL);
  else { LWARN("keypad: service already running"); }
  assert(service);
  return 0;
}

bool is_keypad_service_running(void) { return !!service; }

void shutdown_keypad_service() {
  if (service) zactor_destroy(&service);
  else { LWARN("keypad: service is not running"); }
  service = NULL;
}
