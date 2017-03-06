#include <inttypes.h>
#include <time.h>
#include "config.h"
#include "services.h"

static zactor_t *service = NULL;

long long current_time_ms(void) {
  long            ms; // Milliseconds
  time_t          s;  // Seconds
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  s  = spec.tv_sec;
  ms = (long)(spec.tv_nsec / 1.0e6); // Convert nanoseconds to milliseconds
  return (long long) s * 1000 + ms;
}

typedef struct {
  long long start_ms;
  long long end_ms;
  char id[32];
} ltimer_t;

static void timer_service(zsock_t *pipe, void *arg) {
  (void) arg;
  int ms_until_next_timer;
  ltimer_t *timers = NULL;
  int num_timers = 0;
  zsock_t *bcast  = zsock_new_pub(TIMER_BCAST);
  zsock_t *server = zsock_new_rep(TIMER_REQUEST);
  zpoller_t *poller = zpoller_new(pipe, server, NULL);
  int next_id = 0;
  zsock_signal(pipe, 0);

  while (1) {
    int i;
    long long time;
    char server_response[32];
    server_response[0] = '\0';

    // figure out how long until the next timer is up
    ms_until_next_timer = -1;
    for (i = 0; i < num_timers; i++) {
      long long difference = timers[i].end_ms - timers[i].start_ms;
      if (ms_until_next_timer == -1 || difference < ms_until_next_timer)
        ms_until_next_timer = (int) difference;
    }
    // wait up to that many milliseconds (or forever, if there are not timers)
    // to receive some communication. If we get communication, it's to create
    // a timer or shut down. Regardless if we get comm or timeout, we should
    // check if any timers have expired.
    void *in = zpoller_wait(poller, ms_until_next_timer);
    time = current_time_ms();
    if (!in) {
      if (!zpoller_expired(poller)) {
        LWARN("timer: interrupted!");
        break;
      }
    } else if (in == pipe) {
      LINFO("timer: shutting down");
      break;
    } else if (in == server) {
      int delay;
      zsock_recv(server, "i", &delay);
      ltimer_t *timer = NULL;
      for (i = 0; i < num_timers; i++) {
        if (strlen(timers[i].id) == 0) {// unused timer slot
          timer = &(timers[i]);
          break;
        }
      }
      if (timer == NULL) {
        timers = (ltimer_t *) realloc(timers, (num_timers + 1) * sizeof(ltimer_t));
        timer = &(timers[num_timers++]);
      }
      timer->start_ms = time;
      timer->end_ms   = time + delay;
      sprintf(timer->id, "timer:%d", ++next_id);
      sprintf(server_response, "%s", timer->id);
      LDEBUG("timer: created timer %s with %dms delay (start: %lld, end: %lld)", timer->id, delay, timer->start_ms, timer->end_ms);
    }

    for (i = 0; i < num_timers; i++) {
      if (timers[i].id[0] != '\0' && timers[i].end_ms <= time) {
        char start_ms[64], end_ms[64];
        sprintf(start_ms, "%lld", timers[i].start_ms);
        sprintf(end_ms,   "%lld", timers[i].end_ms);
        zsock_send(bcast, "sssss", timers[i].id, "started_at", start_ms, "ended_at", end_ms);
        LDEBUG("timer: %s expired", timers[i].id);
        timers[i].id[0] = '\0'; // mark as expired so we can reuse the slot
      }
    }

    // defer sending any response to a timer create request, to make sure that
    // negligible timers (0ms, etc) have a chance to fire immediately. This
    // is basically a HACK to support automated execution, e.g. unit tests.
    if (server_response[0] != '\0') {
      zsock_send(server, "s", server_response);
      server_response[0] = '\0';
    }
  }

  if (timers != NULL) free(timers);
  zsock_destroy(&server);
  zsock_destroy(&bcast);
  zpoller_destroy(&poller);
}

int init_timer_service(void) {
  if (!service) service = zactor_new(timer_service, NULL);
  return 0;
}

void shutdown_timer_service(void) {
  if (service) zactor_destroy(&service);
  service = NULL;
}
