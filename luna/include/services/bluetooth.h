#ifndef SERVICES_BLUETOOTH_H
#define SERVICES_BLUETOOTH_H

#define BLUETOOTH_ENDPOINT "inproc://bluetooth"

#ifdef  __cplusplus
extern "C" {
#endif

  int  init_bluetooth_service(void);
  void shutdown_bluetooth_service(void);

#ifdef __cplusplus
}
#endif

#endif // SERVICES_BLUETOOTH_H
