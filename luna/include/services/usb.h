#ifndef SERVICES_USB_H
#define SERVICES_USB_H

#ifdef  __cplusplus
extern "C" {
#endif

  int  init_usb_service(void);
  void shutdown_usb_service(void);

#ifdef __cplusplus
}
#endif

#endif // SERVICES_USB_H
