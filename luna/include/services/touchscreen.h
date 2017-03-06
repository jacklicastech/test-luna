/* 
 * File:   https_service.h
 * Author: Colin
 *
 * Created on August 1, 2016, 4:07 PM
 */

#ifndef TOUCHSCREEN_SERVICE_H
#define TOUCHSCREEN_SERVICE_H

#ifdef  __cplusplus
extern "C" {
#endif

#define TOUCH_ENDPOINT "inproc://touch"

int init_touchscreen_service(void);
void shutdown_touchscreen_service(void);


#ifdef  __cplusplus
}
#endif

#endif  /* TOUCHSCREEN_SERVICE_H */

