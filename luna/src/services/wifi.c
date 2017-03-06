#include "config.h"
#ifdef HAVE_CTOS
    #include <ctosapi.h>
#endif

#include "services.h"
#include "io/signals.h"

#define BLOCKING_WAIT_TIME 1    // seconds
#define WIFI_POLL_INTERVAL 5000 // milliseconds

static zactor_t *service = NULL;

#ifdef HAVE_CTOS
static int load_wifi_credentials(char **ssid, char **pass) {
    int err = 0;
    zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
    if (!settings) return 1;
    err = settings_get(settings, 2, "wifi.ssid", "wifi.password", ssid, pass);
    zsock_destroy(&settings);
    return err;
}

/*
 * Loops for as long as the wifi is in a "processing" state, then returns
 * the first non-processing error code that was encountered. The initial
 * state is meant to be returned from a prior wifi interaction. If it's
 * anything other than d_WIFI_IO_PROCESSING, the initial state will be
 * returned.
 */
static unsigned short block_until_wifi_result(unsigned short initial_state) {
    unsigned int status; // not inspected
    unsigned short code = initial_state;
    while (code == d_WIFI_IO_PROCESSING) {
        sleep(BLOCKING_WAIT_TIME);
        code = CTOS_WifiStatus(&status);
    }
    return code;
}

/* Handles scanning for devices and reacting to wifi state changes */
static void wifi_service(zsock_t *pipe, void *args) {
    zsock_t *notify       = zsock_new_pub(WIFI_CHANGED_ENDPOINT);
    zsock_t *ssid_changed = zsock_new_sub(SETTINGS_CHANGED_ENDPOINT, "wifi.ssid");
    zsock_t *pass_changed = zsock_new_sub(SETTINGS_CHANGED_ENDPOINT, "wifi.password");
    zpoller_t *poller = zpoller_new(pipe, ssid_changed, pass_changed, NULL);
    char *channel = NULL, *ssid = NULL, *password = NULL;
    unsigned short res;
    unsigned char numInfos;
    CTOS_stWifiInfo *infos;
    int i;
    unsigned int state;
    int prev_conn_state = WIFI_CONNECTION_STATE_DISCONNECTED, prev_txrx_state = 0;
    int current_conn_strength = 0; // only nonzero when connecting/connected
    BYTE ip[128];
    //bool scanned = false;
    bool first = true;
    
    if ((res = CTOS_WifiOpen()) != d_OK) {
        LERROR("wifi: failed to open WiFi interface; not proceeding");
        zsock_signal(pipe, SIGNAL_INIT_FAILED);
        return;
    }

    if (load_wifi_credentials(&ssid, &password)) {
        LWARN("wifi: failure while querying initial SSID and password values");
    }

    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);
    LINFO("wifi: service initialized");

    
    while (1) {
        void *in = NULL;
        
        // Don't poll on first iteration, so that wifi can be initialized
        // without first waiting for timeout to expire
        if (!ssid || !first) {
            // loop to ensure all pending messages are received
            while ((in = zpoller_wait(poller, WIFI_POLL_INTERVAL))) {
                if (in == pipe) {
                    LINFO("wifi: received shutdown signal");
                    goto service_shutdown;
                }

                if (in == ssid_changed || in == pass_changed) {
                    if (in == ssid_changed) {
                        if (ssid) free(ssid);
                        ssid = NULL;
                        if (zsock_recv(ssid_changed, "ss", &channel, &ssid)) {
                            free(channel);
                            LWARN("wifi: failed to receive new SSID");
                            continue;
                        }
                        free(channel);
                        LDEBUG("wifi: SSID changed to '%s'", ssid);
                    } else {
                        if (password) free(password);
                        password = NULL;
                        if (zsock_recv(pass_changed, "ss", &channel, &password)) {
                            free(channel);
                            LWARN("wifi: failed to receive new password");
                            continue;
                        }
                        free(channel);
                        LDEBUG("wifi: password changed to '%s'", password);
                    }

                    LDEBUG("wifi: disconnecting and waiting for the next refresh");
                    if ((res = block_until_wifi_result(CTOS_WifiDisconnectAP())) != d_OK) {
                        LWARN("wifi: failed to disconnect from AP: %u", res);
                    }
                }
            }

            if (!zpoller_expired(poller) && zmq_errno() != EAGAIN) {
                LWARN("wifi: service interrupted: %s", zmq_strerror(zmq_errno()));
                goto service_shutdown;
            }
        }
        first = false;
        
        if ((block_until_wifi_result(d_WIFI_IO_PROCESSING)) != d_OK) {
            LWARN("wifi: a recent operation ended in failure");
        }

        if (prev_conn_state == WIFI_CONNECTION_STATE_DISCONNECTED) {
            // try to connect to a network
            if ((res = block_until_wifi_result(CTOS_WifiScan())) != d_OK) {
                LWARN("wifi: failed to scan wifi devices: %u", res);
            }

            if ((res = CTOS_WifiInfoGet(&numInfos, &infos)) != d_OK) {
                LWARN("wifi: error %u while getting wifi infos", res);
                continue;
            }

            for (i = 0; i < numInfos; i++) {
                zmsg_t *msg = zmsg_new();
                LDEBUG("wifi: publishing refresh for SSID '%s' (frequency: %s, current strength: %s, encryption: %s)", infos[i].ESSID, infos[i].Freq, infos[i].Quality, infos[i].EncryptionKey);
                zmsg_addstr(msg, WIFI_ACCESS_POINT_REFRESHED);
                zmsg_addstr(msg, "address");
                zmsg_addmem(msg, (const char *) infos[i].Address, sizeof(infos[i].Address));
                zmsg_addstr(msg, "ssid");
                zmsg_addmem(msg, (const char *) infos[i].ESSID, sizeof(infos[i].ESSID));
                zmsg_addstr(msg, "mode");
                zmsg_addmem(msg, (const char *) infos[i].Mode, sizeof(infos[i].Mode));
                zmsg_addstr(msg, "freq");
                zmsg_addmem(msg, (const char *) infos[i].Freq, sizeof(infos[i].Freq));
                zmsg_addstr(msg, "quality");
                zmsg_addmem(msg, (const char *) infos[i].Quality, sizeof(infos[i].Quality));
                zmsg_addstr(msg, "signal");
                zmsg_addmem(msg, (const char *) infos[i].SignalLv, sizeof(infos[i].SignalLv));
                zmsg_addstr(msg, "noise");
                zmsg_addmem(msg, (const char *) infos[i].NoiseLv, sizeof(infos[i].NoiseLv));
                zmsg_addstr(msg, "encryption");
                zmsg_addmem(msg, (const char *) infos[i].EncryptionKey, sizeof(infos[i].EncryptionKey));
                zmsg_addstr(msg, "type");
                zmsg_addmem(msg, (const char *) infos[i].Type_1, sizeof(infos[i].Type_1));
                zmsg_send(&msg, notify);

                if (ssid != NULL && !strncmp((char *) (infos[i].ESSID), ssid, 36)) {
                    current_conn_strength = atoi((char *) infos[i].Quality);
                    if (prev_conn_state == WIFI_CONNECTION_STATE_DISCONNECTED) {
                        LDEBUG("wifi: new AP matches SSID, and not currently connected, so trying to connect to it");
                        if (password != NULL) {
                            if (!strcmp((char *) infos[i].EncryptionKey, "on")) {
                                LDEBUG("wifi:   ... with a password");
                                res = block_until_wifi_result(CTOS_WifiConnectAP(infos+i, (unsigned char *) password, strlen(password)));
                            } else {
                                LDEBUG("wifi:   ... but ignoring the password because encryption is disabled");
                                res = block_until_wifi_result(CTOS_WifiConnectAP(infos+i, NULL, 0));
                            }
                        } else {
                            LDEBUG("wifi:   ... without a password");
                            res = block_until_wifi_result(CTOS_WifiConnectAP(infos+i, NULL, 0));
                        }

                        if (res != d_OK) {
                            LWARN("wifi: error %u while trying to connect to AP", res);
                        }
                    } else {
//                            LDEBUG("wifi: new AP matches SSID, but we are already connected or connecting, so ignoring it");
                    }
                } else {
//                        LDEBUG("wifi: new AP does not match SSID, so ignoring it");
                }
            }
        }

//            free(infos);

        if ((res = CTOS_WifiStatus(&state)) != d_OK) {
            LWARN("wifi: error %u while getting wifi status", res);
            continue;
        }
        
        block_until_wifi_result(res);

        int conn = WIFI_CONNECTION_STATE_DISCONNECTED, txrx = 0;
//            if (state & d_WIFI_STATE_CONNECTING || state & d_WIFI_STATE_AP_CONNECTING) {
//                conn = WIFI_CONNECTION_STATE_CONNECTING;
//            } else
        if (state & d_WIFI_STATE_AP_CONNECTED) {
            conn = WIFI_CONNECTION_STATE_CONNECTED;
        } else {
            // disconnected, signal strength is 0
            current_conn_strength = 0;
        }

        if (conn != prev_conn_state) {
            LDEBUG("wifi: connection state has changed (was %d, is %d)", prev_conn_state, conn);
            prev_conn_state = conn;
            memset(ip, 0, sizeof(ip));
            if (conn == WIFI_CONNECTION_STATE_CONNECTED) {
                BYTE size = sizeof(ip);
                if ((res = CTOS_WifiConfigGet(d_WIFI_CONFIG_IP, ip, &size)) != d_OK) {
                    LERROR("wifi: error %u while querying IP address", res);
                    continue;
                }

                LINFO("wifi: connected: IP is %s", ip);
            }
        }

        // refresh connection signal strength
        unsigned char quality = 0;
        if (prev_conn_state == WIFI_CONNECTION_STATE_CONNECTED) {
            if ((res = block_until_wifi_result(CTOS_WifiQualityGet(&quality))) == d_OK) {
                current_conn_strength = quality;
                LDEBUG("wifi: connection strength is now %d", current_conn_strength);
            }
        }
        
        // resend connection state because it's worth retransmitting in order
        // to keep conn strength up to date. This way subscribers can avoid
        // a ton of logic trying to determine conn strength from the 
        // broadcasted infos.
        const char *connstr;
        switch(conn) {
            case WIFI_CONNECTION_STATE_CONNECTED: connstr = "connected"; break;
            case WIFI_CONNECTION_STATE_DISCONNECTED: connstr = "disconnected"; break;
            case WIFI_CONNECTION_STATE_CONNECTING: connstr = "connecting"; break;
            default: LERROR("wifi: BUG: unexpected conn state: %d", conn); connstr = "unknown";
        }
        LDEBUG("sending wifi state change");
        if (zsock_send(notify, "ssssssi", WIFI_CONNECTION_STATE_CHANGED, "state", connstr, "ip", ip, "strength", current_conn_strength)) {
            LWARN("wifi: failed to publish connection state change");
            continue;
        }

        LDEBUG("sending wifi transmit change");
        if (state & d_WIFI_STATE_SENDING)   txrx |= WIFI_TRANSMIT_STATE_SENDING;
        if (state & d_WIFI_STATE_RECEIVING) txrx |= WIFI_TRANSMIT_STATE_RECEIVING;
        if (txrx != prev_txrx_state) {
            LDEBUG("wifi: transmit state has changed (was %d, is %d)", prev_txrx_state, txrx);
            prev_txrx_state = txrx;
            zsock_send(notify, "ssi", WIFI_TRANSMIT_STATE_CHANGED, "flags", txrx);
        }
    }
    
service_shutdown:
    LDEBUG("wifi: service terminating");
    zpoller_destroy(&poller);
    zsock_destroy(&ssid_changed);
    zsock_destroy(&pass_changed);
    zsock_destroy(&notify);
    if ((res = CTOS_WifiClose()) != d_OK) {
        LWARN("wifi: failed to close wifi interface");
    }

    LINFO("wifi: service terminated");
    zsock_signal(pipe, SIGNAL_NO_ERROR);
}
#endif // HAVE_CTOS

int init_wifi_service(void) {
#if HAVE_CTOS
    service = zactor_new(wifi_service, NULL);
#else // HAVE_CTOS
    LWARN("wifi: not supported on this device");
#endif // HAVE_CTOS
    return 0;
}

void shutdown_wifi_service(void) {
    if (service) zactor_destroy(&service);
}
