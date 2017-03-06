/* 
 * File:   transactions_service.h
 * Author: Colin
 *
 * Created on December 18, 2015, 2:26 AM
 */

#ifndef SETTINGS_SERVICE_H
#define SETTINGS_SERVICE_H

#ifdef  __cplusplus
extern "C" {
#endif

#include "czmq.h"
#include "util/uthash.h"
    
#define SETTINGS_ENDPOINT         "inproc://settings"
#define SETTINGS_CHANGED_ENDPOINT "inproc://settings-changed"
int init_settings_service(void);
void shutdown_settings_service(void);
int settings_get(zsock_t *getset, int num, ...);
int settings_set(zsock_t *getset, int num, ...);
int settings_del(zsock_t *getset, int num, ...);
int settings_purge(zsock_t *getset);
zmsg_t *settings_getall(zsock_t *getset);

/*
 * The service running at SETTINGS_ENDPOINT is a request/reply service.
 * Consumers should perform an initial request to the endpoint. The request
 * format can take one of the following forms:
 * 
 *      1. The first frame contains a GET value and there is no second frame.
 *         The response will be N*2 frames where frame N is the name of a
 *         setting and frame N+1 is its current value.
 *
 *      2. The first frame contains a GET value and subsequent frames list the
 *         settings which are to be queried. More than 1 frame may be queried.
 *
 *      3. The request contains an odd number of frames, where frame 1 is a
 *         SET value, frame 2 is the name of a setting, frame 3 is the new
 *         value of the setting, and so on for all the settings which are to
 *         be changed. The response will be an OK signal or an error signal if
 *         the new settings are invalid. If any setting fails to be assigned,
 *         no settings will be changed.
 * 
 *
 * The service running at SETTINGS_CHANGED_ENDPOINT is a pub/sub service.
 * Consumers may subscribe to notifications at this service in order to
 * receive a notification when any setting is changed. The message will always
 * consist of 2 frames: (1) the name of the setting and (2) its new value.
 */

#define SETTINGS_GET                   0
#define SETTINGS_SET                   1
#define SETTINGS_DEL                   2
#define SETTINGS_PURGE                 3
#define SETTINGS_RESPONSE_OK           0
#define SETTINGS_RESPONSE_ERROR        1

#ifdef  __cplusplus
}
#endif

#endif  /* TRANSACTIONS_SERVICE_H */

