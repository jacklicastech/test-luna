#include "config.h"
#include <fcntl.h>
#include <inttypes.h>
#include <linux/input.h>
#include <time.h>
#include "services.h"

static zactor_t *service = NULL;

#define BUFFER_SIZE 32
#define BATCH_FLUSH_RATE_MS 125 // buffer events for no more than this many ms

#ifndef ABS_MT_TOUCH_MAJOR
  #define ABS_MT_TOUCH_MAJOR      0x30    /* Major axis of touching ellipse */
#endif

#ifndef ABS_MT_TOUCH_MINOR
  #define ABS_MT_TOUCH_MINOR      0x31    /* Minor axis (omit if circular) */
#endif

#ifndef ABS_MT_POSITION_X
  #define ABS_MT_POSITION_X       0x35    /* Center X ellipse position */
#endif

#ifndef ABS_MT_POSITION_Y
  #define ABS_MT_POSITION_Y       0x36    /* Center Y ellipse position */
#endif

#ifndef ABS_MT_TRACKING_ID
  #define ABS_MT_TRACKING_ID      0x39    /* Unique ID of initiated contact */
#endif

static void touchscreen_service(zsock_t *pipe, void *arg) {
  (void) arg;
  zsock_t *bcast    = zsock_new_pub(TOUCH_ENDPOINT);
  zpoller_t *poller = zpoller_new(pipe, NULL);
  unsigned char event_name[256];
  struct input_event in_ev[BUFFER_SIZE];
  fd_set rd_wait_fd;
  struct timeval tv;
  int tp_fd = -1;
  int batch_len = 0;
  char *buffer = calloc(1, sizeof(char) * 1024);
  size_t buffer_len = 1024;
  zsock_signal(pipe, 0);

  if (!bcast) {
    LERROR("touchscreen: could not create broadcast socket");
    goto shutdown;
  }

  tp_fd = open("/dev/input/event0", O_RDONLY);
  if(tp_fd < 0) {
    LERROR("touchscreen: could not open touch screen device");
    goto shutdown;
  }

  // get the name of this input event
  memset(event_name, 0, sizeof(event_name));
  if(ioctl(tp_fd, EVIOCGNAME(sizeof(event_name)), event_name) < 0) {
    close(tp_fd);
    LERROR("touchscreen: failed to get the device event name");
    goto shutdown;
  }

  LTRACE("touchscreen: event name: %s", event_name);
  tv.tv_sec = 0;
  tv.tv_usec = BATCH_FLUSH_RATE_MS * 1000;
  while (1) {
    void *in = zpoller_wait(poller, 0);
    if (!in) {
      if (!zpoller_expired(poller)) {
        LWARN("touchscreen: interrupted!");
        break;
      }

      // If we get here then there are no messages to process, so check for
      // input events.
      FD_ZERO(&rd_wait_fd);
      FD_SET(tp_fd, &rd_wait_fd);
      int rd_cnt = 0;
      int res = select(tp_fd + 1, &rd_wait_fd, NULL, NULL, &tv);
      if (res < 0) {
        LERROR("touchscreen: error while waiting for events: %s", strerror(errno));
        continue;
      } else if (res == 0) {
        // LTRACE("touchscreen: timed out waiting for events");
      } else {
        rd_cnt = read(tp_fd, in_ev, sizeof(struct input_event) * BUFFER_SIZE);
        if ((rd_cnt == -1) || (rd_cnt < sizeof(struct input_event))) {
          LWARN("touchscreen: failed to read events");
          rd_cnt = 0;
        }
      }

      // aggregate the buffered events into a single packet that we'll then
      // broadcast when we get a synchronization event
      const char *type = "update"; // NULL;
      int x = -1, y = -1, pressure = -1;
      int i;
      for (i = 0; i < rd_cnt / sizeof(struct input_event); i++) {
        switch(in_ev[i].type) {
          case EV_KEY:
            if(in_ev[i].code == BTN_TOUCH) {
              if (in_ev[i].value == 1) {
                type = "start";
              } else {
                type = "stop";
              }
            } else {
              // LTRACE("touchscreen: ignoring unexpected event code: %d", in_ev[i].code);
            }
            break;
          case EV_ABS:
            switch(in_ev[i].code) {
              case ABS_X: x = in_ev[i].value; break;
              case ABS_Y: y = in_ev[i].value; break;
              case ABS_PRESSURE: pressure = in_ev[i].value; break;
              case ABS_MT_POSITION_X: break;
              case ABS_MT_POSITION_Y: break;
              case ABS_MT_TOUCH_MAJOR: break;
              case ABS_MT_TOUCH_MINOR: break;
              default:
                LTRACE("touchscreen: ignoring unexpected EV_ABS code: %d", in_ev[i].code);
            }
            break;
          case EV_SYN:
            assert(type != NULL);
            // we can get sync events constantly, even if there were no actual
            // changes in between. Only send an event if something changed.
            if ((x >= 0 && y >= 0) || strcmp(type, "update")) {
              // emit event
              batch_len++;
              if (buffer_len - strlen(buffer) < 64) {
                buffer_len += 1024;
                buffer = realloc(buffer, buffer_len * sizeof(char));
              }
              sprintf(buffer + strlen(buffer), "%s,%d,%d,%d|", type, x, y, pressure);

              x = -1;
              y = -1;
              pressure = -1;
              type = "update";
            }
            break;
          default:
            LTRACE("touchscreen: ignoring unexpected event type: %d", in_ev[i].type);
        }
      }

      if (tv.tv_usec <= 1000) {
        tv.tv_usec = BATCH_FLUSH_RATE_MS * 1000;
        if (batch_len > 0) {
          char batch_size[32];
          sprintf(batch_size, "%d", batch_len);
          zmsg_t *msg = zmsg_new();
          zmsg_pushstr(msg, buffer);
          zmsg_pushstr(msg, "batch");
          zmsg_pushstr(msg, batch_size);
          zmsg_pushstr(msg, "batch_size");
          zmsg_pushstr(msg, "touch");
          zmsg_send(&msg, bcast);
          *buffer = '\0';
          batch_len = 0;
        }
      }
    } else if (in == pipe) {
      LINFO("touchscreen: shutting down");
      break;
    }
  }

shutdown:
  free(buffer);
  if (bcast) zsock_destroy(&bcast);
  if (tp_fd >= 0) close(tp_fd);
  zpoller_destroy(&poller);
  LTRACE("touchscreen: shutdown complete");
}

int init_touchscreen_service(void) {
  if (!service) service = zactor_new(touchscreen_service, NULL);
  return 0;
}

void shutdown_touchscreen_service(void) {
  if (service) zactor_destroy(&service);
  service = NULL;
}
