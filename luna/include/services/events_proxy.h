#ifndef SERVICES_EVENTS_PROXY_H
#define SERVICES_EVENTS_PROXY_H

#ifdef  __cplusplus
extern "C" {
#endif

  #define EVENTS_PUB_ENDPOINT "inproc://events/pub"
  #define EVENTS_SUB_ENDPOINT "inproc://events/sub"
  int  init_events_proxy_service(void);
  void shutdown_events_proxy_service(void);

#ifdef __cplusplus
}
#endif

#endif // SERVICES_EVENTS_PROXY_H
