#ifndef SERVICES_INPUT_H
#define SERVICES_INPUT_H

// battery may not strictly be an input, but it needs to be polled like one,
// so we'll save CPU and conserve power by polling it at the same time as the
// other inputs.
#define INPUT_BATTERY_ENDPOINT "inproc://battery"
#define INPUT_KEYPAD_ENDPOINT  "inproc://keypad"
#define INPUT_MSR_ENDPOINT     "inproc://msr"

#ifdef  __cplusplus
extern "C" {
#endif

  int  init_input_service(void);
  void shutdown_input_service(void);

#ifdef __cplusplus
}
#endif

#endif // SERVICES_INPUT_H
