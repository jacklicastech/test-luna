/* 
 * File:   wifi_service.h
 * Author: Colin
 *
 * Created on December 29, 2015, 3:20 AM
 */

#ifndef WIFI_SERVICE_H
#define	WIFI_SERVICE_H

#ifdef	__cplusplus
extern "C" {
#endif

#define WIFI_CHANGED_ENDPOINT "inproc://wifi-changed"

// Channels that may be published to
//#define WIFI_ACCESS_POINT_DISCOVERED  "ap-discovered"
//#define WIFI_ACCESS_POINT_LOST        "ap-lost"
#define WIFI_ACCESS_POINT_REFRESHED   "ap-refreshed"
#define WIFI_CONNECTION_STATE_CHANGED "connection-state-changed"
#define WIFI_TRANSMIT_STATE_CHANGED   "transmit-state-changed"
    
#define WIFI_CONNECTION_STATE_DISCONNECTED 1
#define WIFI_CONNECTION_STATE_CONNECTING   2
#define WIFI_CONNECTION_STATE_CONNECTED    3
    
#define WIFI_TRANSMIT_STATE_SENDING   1
#define WIFI_TRANSMIT_STATE_RECEIVING 2


int init_wifi_service(void);
void shutdown_wifi_service(void);

/*
 * The service running at WIFI_CHANGED_ENDPOINT is a pub/sub service. Consumers
 * can subscribe to this service to be notified about changes to the state of
 * the WiFi connection. Messages will be published to the following channels:
 * 
 *     WIFI_ACCESS_POINT_REFRESHED:
 *         Notifies subscribers of changes to an access point. This
 *         is useful for reporting on signal strength variance, for example.
 *         Message payload is a `CTOS_stWifiInfo`.
 * 
 *     WIFI_CONNECTION_STATE_CHANGED:
 *         Notifies subscribers that the state of the local WiFi network
 *         interface has changed. Message payload consists of:
 *           Frame 0: Integer: the new state, which is one of:
 *                    WIFI_STATE_CONNECTING, WIFI_STATE_CONNECTED,
 *                    WIFI_STATE_DISCONNECTED
 *           Frame 1: String: the IP address if the new state is
 *                    WIFI_STATE_CONNECTED; an empty string otherwise.
 *           Frame 2: Integer: a number representing the signal strength to the
 *                    currently-connected or currently-connecting access point,
 *                    in the range 0..5; or 0, if disconnected.
 *
 *     WIFI_TRANSMIT_STATE_CHANGED:
 *         Notifies subscribers that the transmit and/or receive state has been
 *         changed. Either the WiFi has started transmitting, stopped
 *         transmitting, started receiving, or stopped receiving. Message
 *         payload is a bitwise OR of WIFI_TRANSMIT_STATE_SENDING and
 *         WIFI_TRANSMIT_STATE_RECEIVING.
 * 
 */


#ifdef	__cplusplus
}
#endif

#endif	/* WIFI_SERVICE_H */

