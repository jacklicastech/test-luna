#ifndef SERVICES_TIMER_H
#define SERVICES_TIMER_H

#ifdef  __cplusplus
extern "C" {
#endif

  #define TIMER_REQUEST "inproc://timers"
  #define TIMER_BCAST   "inproc://timer-expired"

  int  init_timer_service(void);
  void shutdown_timer_service(void);

#ifdef __cplusplus
}
#endif

#endif // SERVICES_TIMER_H
