#include "config.h"
#include <stdio.h>
#include "services.h"

static zactor_t *service = NULL;

int init_events_proxy_service() {
  if (!service) {
    service = zactor_new(zproxy, NULL);
    assert(service);
    zstr_sendx(service, "VERBOSE", NULL);
    zsock_wait(service);
    zstr_sendx(service, "FRONTEND", "XSUB", EVENTS_PUB_ENDPOINT, NULL);
    zstr_sendx(service, "BACKEND",  "XPUB", EVENTS_SUB_ENDPOINT, NULL);
    LINFO("events-proxy: initialized");
  } else {
    LWARN("events-proxy: service already running");
  }
  assert(service);
  return 0;
}

void shutdown_events_proxy_service() {
  if (service) {
    zactor_destroy(&service);
    LINFO("events-proxy: service terminated");
  } else {
    LWARN("events-proxy: service is not running");
  }
  service = NULL;
}
