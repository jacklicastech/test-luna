#include "config.h"
#include <stdio.h>
#include <czmq.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <resolv.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "services.h"
#include "services.h"
#include "util/machine_id.h"
#include "util/files.h"

#define FAIL    -1
#define ACCEPT_TIMEOUT 10 // ms
#define BEACON_BROADCAST_INTERVAL 3000 // ms

// see https_request.c
void https_api_handle_request(zsock_t *pipe, void *args);

static zactor_t *webserver_actor = NULL;

/* used to restart the web server when settings change that require doing so */
static zactor_t *webserver_monitor = NULL;

/* socket fd used to broadcast this device's name and REST URL */
static int bcast_sock = -1;

static int maybe_reset_webserver_port(int portn) {
    char tmp[6];

    if (portn < 1024 || portn > 65535) {
        zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
        // can't host on lower ports without root permissions
        LWARN("webserver: port setting is unacceptable, resetting it to %d", DEFAULT_WEBSERVER_PORT);
        portn = DEFAULT_WEBSERVER_PORT;
        sprintf(tmp, "%d", portn);
        settings_set(settings, 1, "webserver.port", tmp);
        zsock_destroy(&settings);
    }

    return portn;
}

static int poll_webserver_port() {
    zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
    char *port = NULL;
    int portn = DEFAULT_WEBSERVER_PORT;

    settings_get(settings, 1, "webserver.port", &port);
    if (port && strlen(port)) {
        portn = atoi(port);
        portn = maybe_reset_webserver_port(portn);
    }
    if (port) free(port);
    zsock_destroy(&settings);

    return portn;
}

static int ssl_bind(int port) {
    int sd;
    int one = 1;
    struct sockaddr_in addr;

    sd = socket(PF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one));
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sd, (struct sockaddr *)(&addr), sizeof(addr)) != 0) {
        LERROR("webserver: port bind failed: %s", strerror(errno));
        return -1;
    }
    if (listen(sd, 10) != 0) {
        LERROR("webserver: listen failed: %s", strerror(errno));
        return -1;
    }
    
    int flags = fcntl(sd, F_GETFL, 0);
    assert(flags != -1);
    fcntl(sd, F_SETFL, flags | O_NONBLOCK);
    
    return sd;
}

static void log_ssl_cipher_list(SSL_CTX *ctx) {
    SSL *ssl = SSL_new(ctx);
    int i;
    STACK_OF(SSL_CIPHER) *sk = SSL_get_ciphers(ssl);
    LINFO("webserver: ssl: available ciphers:");
    for (i = 0; sk && i < sk_SSL_CIPHER_num(sk); i++) {
        LINFO("webserver: ssl:   %s", sk_SSL_CIPHER_value(sk, i)->name);
    }
    SSL_shutdown(ssl); // see https://john.nachtimwald.com/2014/10/05/server-side-session-cache-in-openssl/
    SSL_free(ssl);                                  /* release SSL state */
}

static SSL_CTX *init_ssl_context(void) {
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = SSLv23_server_method();            /* Try SSLv3 but fall back to SSLv2 */
    ctx = SSL_CTX_new(method);			/* create new context from method */
    if (ctx == NULL) {
        LERROR("webserver: failed to initialize SSL context: %s", ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    if (SSL_CTX_set_cipher_list(ctx, LUNA_SSL_CIPHER_LIST) != 1) {
        LERROR("webserver: failed to set SSL cipher list: %s", ERR_error_string(ERR_get_error(), NULL));
        return NULL;
    }

    #if defined(OpenSSL_version) // openssl > 1.0.2
        LINFO("%s - %s", OpenSSL_version(OPENSSL_VERSION), OpenSSL_version(OPENSSL_PLATFORM));
    #else
        LINFO("%s - %s", SSLeay_version(SSLEAY_VERSION), SSLeay_version(SSLEAY_PLATFORM));
    #endif

    log_ssl_cipher_list(ctx);

    #if defined(SSL_CTX_set_ecdh_auto) // openssl 1.0.2+
        SSL_CTX_set_ecdh_auto(ctx, 1);
    #else
        EC_KEY *key = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        SSL_CTX_set_tmp_ecdh(ctx, key);
        EC_KEY_free(key);
    #endif

    SSL_CTX_set_session_cache_mode(ctx, SSL_SESS_CACHE_SERVER);
    return ctx;
}

static int load_ssl_certs(SSL_CTX* ctx, const char *CertFile, const char *KeyFile) {
    char *buf;

    /* set the certificate chain from CertFile. Note that this file must
     * contain the entire cert chain up to the root CA, in order for some
     * clients to be able to validate it (iOS is a known offender). */
    assert(buf = find_readable_file(NULL, CertFile));
    if (SSL_CTX_use_certificate_chain_file(ctx, buf) != 1) {
        LERROR("webserver: failed to load SSL certificate: %s", ERR_error_string(ERR_get_error(), buf));
        free(buf);
        return 1;
    }
    free(buf);

    /* set the private key from KeyFile (may be the same as CertFile) */
    assert(buf = find_readable_file(NULL, KeyFile));
    if (SSL_CTX_use_PrivateKey_file(ctx, buf, SSL_FILETYPE_PEM) != 1) {
        LERROR("webserver: failed to load SSL key: %s", ERR_error_string(ERR_get_error(), buf));
        free(buf);
        return 2;
    }
    free(buf);

    /* verify private key */
    if (SSL_CTX_check_private_key(ctx) != 1) {
        // treat it as a warning, the owner may be doing it on purpose
        // (we do this in dev)
        LWARN("webserver: private key does not match the public certificate");
        return 0;
    }
    return 0;
}

/* Broadcasts the name and URL of this REST API to the network. */
static void broadcast(const char *name, const char *port, const char *address,
                      int battery_percentage, int is_battery_charging) {
    int portn = atoi(port);
    char *message;
    struct sockaddr_in s;
    char *id;

    if (bcast_sock < 0) return;

    id = unique_machine_id();
    message = (char *) calloc(strlen(name) + strlen(address) + 256, sizeof(char));
    const char *BEACON_MSG_FMT = "{\"name\":\"%s\",\"id\":\"%s\",\"address\":\"%s\",\"battery\":{\"percentage\":%d,\"charging\":%s}}";
    sprintf(message, BEACON_MSG_FMT, name, id, address, battery_percentage, is_battery_charging == 0 ? "false" : "true");
    free(id);

    memset(&s, 0, sizeof(struct sockaddr_in));
    s.sin_family = AF_INET;
    s.sin_port = (in_port_t)htons(portn);
    s.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    if(sendto(bcast_sock, message, strlen(message), 0,
              (struct sockaddr *) &s, sizeof(struct sockaddr_in)) < 0)
        LWARN("webserver: beacon: transmit failed: %s", strerror(errno));
    free(message);
}

static void webserver_service(zsock_t *pipe, void *args) {
    SSL_CTX *ctx = NULL;
    int server = -1;
    zpoller_t *requests_in_progress = zpoller_new(pipe, NULL);
    int num_in_progress = 0;
    bool running = true;
    fd_set fdset;
    struct timeval accept_timeout;
    zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
    char *device_name = NULL, *broadcast_port = NULL, *broadcast_enabled = NULL;
    char *address = NULL;
    int webserver_port = poll_webserver_port();
    long broadcast_timer = 0;
    int battery_percentage = 0, is_battery_charging = 0;
    zsock_t *battery_events            = zsock_new_sub(INPUT_BATTERY_ENDPOINT,    "");
    zsock_t *wifi_connection_changed   = zsock_new_sub(WIFI_CHANGED_ENDPOINT, WIFI_CONNECTION_STATE_CHANGED);
    zsock_t *device_name_changed       = zsock_new_sub(SETTINGS_CHANGED_ENDPOINT, "device.name");
    zsock_t *broadcast_port_changed    = zsock_new_sub(SETTINGS_CHANGED_ENDPOINT, "webserver.beacon.port");
    zsock_t *broadcast_enabled_changed = zsock_new_sub(SETTINGS_CHANGED_ENDPOINT, "webserver.beacon.enabled");
    zpoller_t *settings_changed_poller = zpoller_new(device_name_changed,
                                                     broadcast_port_changed,
                                                     broadcast_enabled_changed,
                                                     wifi_connection_changed,
                                                     battery_events,
                                                     NULL);

    settings_get(settings, 3, "device.name", "webserver.beacon.port", "webserver.beacon.enabled",
                              &device_name,  &broadcast_port,     &broadcast_enabled);

    if (!(ctx = init_ssl_context())) {
        LERROR("webserver: SSL context init failed");
        goto shutdown;
    }
    if (load_ssl_certs(ctx, "server.crt", "server.key")) {
        LERROR("webserver: SSL init failed");
        goto shutdown;
    }
    server = ssl_bind(webserver_port);
    if (server == -1) {
        LERROR("webserver: server init failed");
        goto shutdown;
    }
    
    FD_ZERO(&fdset);
    FD_SET(server, &fdset);
    
    LINFO("webserver: server ready");
    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);

    while (running) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        SSL *ssl;
        void *active;
        
        accept_timeout.tv_sec = ACCEPT_TIMEOUT / 1000;
        accept_timeout.tv_usec = (ACCEPT_TIMEOUT % 1000) * 1000;
        
        // we only really care about the timeout; we get all the info we need
        // from the subsequent nonblocking accept
        select(1, &fdset, NULL, NULL, &accept_timeout);

        // check for changes to broadcast settings
        while ((active = zpoller_wait(settings_changed_poller, 0))) {
            char *key, *val;
            if (active == battery_events) {
                zmsg_t *msg = zmsg_recv(battery_events);
                key = zmsg_popstr(msg);
                if (!strcmp(key, "charging-started")) is_battery_charging = 1;
                else if (!strcmp(key, "charging-stopped")) is_battery_charging = 0;
                else if (!strcmp(key, "capacity-changed")) {
                    free(key);
                    key = zmsg_popstr(msg);
                    assert(!strcmp(key, "percentage"));
                    val = zmsg_popstr(msg);
                    battery_percentage = atoi(val);
                    free(val);
                }
                free(key);
                zmsg_destroy(&msg);
            } else if (active == device_name_changed) {
                zsock_recv(device_name_changed, "ss", &key, &val);
                if (device_name) free(device_name);
                device_name = val;
            } else if (active == broadcast_port_changed) {
                zsock_recv(broadcast_port_changed, "ss", &key, &val);
                if (broadcast_port) free(broadcast_port);
                broadcast_port = val;
            } else if (active == broadcast_enabled_changed) {
                zsock_recv(broadcast_enabled_changed, "ss", &key, &val);
                if (broadcast_enabled) free(broadcast_enabled);
                broadcast_enabled = val;
            } else if (active == wifi_connection_changed) {
                char *key, *state, *state_val, *ip, *ip_val,
                     *strength;
                int strength_val;
                if (address) free(address);
                zsock_recv(active, "ssssssi", &key, &state, &state_val, &ip, &ip_val, &strength, &strength_val);
                if (!strcmp(state_val, "connected")) {
                    // 192.168.0.1 => https://ip-192-168-0-1.devices.castlestech.io
                    unsigned int i;
                    address = (char *) calloc(strlen(ip_val) + 64, sizeof(char));
                    for (i = 0; i < strlen(ip_val); i++)
                        if (ip_val[i] == '.')
                            ip_val[i] = '-';
                    sprintf(address, "https://ip-%s.devices.castlestech.io:%d", ip_val, webserver_port);
                } else {
                    address = NULL;
                }
                free(key);
                free(state);
                free(state_val);
                free(ip);
                free(ip_val);
                free(strength);
            }
        }

        broadcast_timer -= ACCEPT_TIMEOUT;
        if (broadcast_timer <= 0 && (
            !strcmp(broadcast_enabled, "true") ||
            !strcmp(broadcast_enabled, "yes")  ||
            !strcmp(broadcast_enabled, "on"))) {
            if (address != NULL) {
                broadcast(device_name, broadcast_port, address, battery_percentage, is_battery_charging);
                broadcast_timer = BEACON_BROADCAST_INTERVAL;
            }
        }
        
        int client = accept(server, (struct sockaddr *)(&addr), &len);
        if (client < 0) {
            if (errno != EAGAIN) {
                LWARN("webserver: while accepting a connection: %s", strerror(errno));
            }
            
            // No clients, check for system messages and clean up old requests
            while ((active = zpoller_wait(requests_in_progress, 0))) {
                // activity from someone, but who?
                if (active == pipe) {
                    // system message
                    LINFO("webserver: received shutdown signal");
                    running = false;
                    break;
                } else {
                    zactor_t *actor = (zactor_t *) active;
                    // a request completed, free the actor
                    LDEBUG("webserver: a request was completed");
                    zsock_wait(active); // consume request-completed signal
                    zpoller_remove(requests_in_progress, active);
                    zactor_destroy(&actor);
                    num_in_progress--;
                    LDEBUG("webserver: the request resources have been freed");
                }
            }
            
            if (active == NULL && !(zpoller_expired(requests_in_progress))) {
                LWARN("webserver: service interrupted");
                running = false;
                break;
            }
        } else {
            LINFO("webserver: received request: %s:%d", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            int timeout = 5000; // 5 seconds
            setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
            ssl = SSL_new(ctx);
            SSL_set_fd(ssl, client);
            zactor_t *request = zactor_new(https_api_handle_request, ssl);
            zpoller_add(requests_in_progress, request);
            num_in_progress++;
            LINFO("webserver: now processing request");
        }
    }
    
shutdown:
    if (address)           free(address);
    if (device_name)       free(device_name);
    if (broadcast_port)    free(broadcast_port);
    if (broadcast_enabled) free(broadcast_enabled);
    zpoller_destroy(&settings_changed_poller);
    zsock_destroy(&settings);
    zsock_destroy(&device_name_changed);
    zsock_destroy(&broadcast_port_changed);
    zsock_destroy(&broadcast_enabled_changed);
    zsock_destroy(&wifi_connection_changed);
    zsock_destroy(&battery_events);
    zpoller_remove(requests_in_progress, pipe);
    while (num_in_progress > 0) {
        LDEBUG("webserver: waiting for %d requests still in progress", num_in_progress);
        void *sock = zpoller_wait(requests_in_progress, -1);
        if (sock) {
            zactor_t *actor = (zactor_t *) sock;
            zpoller_remove(requests_in_progress, sock);
            zsock_wait(sock); // consume request-complete signal
            zactor_destroy(&actor);
            num_in_progress--;
        }
    }
    zpoller_destroy(&requests_in_progress);
    if (server != -1) close(server);
    if (ctx) SSL_CTX_free(ctx);
    LINFO("webserver: service has terminated");
    
    zsock_signal(pipe, SIGNAL_NO_ERROR);
}

static void webserver_monitor_service(zsock_t *pipe, void *arg) {
    zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
    zsock_t *port_changed = zsock_new_sub(SETTINGS_CHANGED_ENDPOINT, "webserver.port");
    zpoller_t *poller = zpoller_new(pipe, port_changed, NULL);
    if (!webserver_actor) zactor_new(webserver_service, NULL);
    int current_webserver_port = poll_webserver_port();
    LINFO("webserver-monitor: initialized");
    zsock_signal(pipe, 0);

    while (1) {
        void *sock = zpoller_wait(poller, -1);
        if (sock == port_changed) {
            // consume message, validate new port number
            char *key, *val;
            int new_webserver_port = current_webserver_port;
            zsock_recv(port_changed, "ss", &key, &val);
            if (val && strlen(val) != 0) {
                new_webserver_port = atoi(val);
                if (new_webserver_port < 1024 || new_webserver_port > 65535) {
                    char tmp[6];
                    LWARN("webserver-monitor: port number changed, but new value %d is invalid, so ignoring it", new_webserver_port);
                    sprintf(tmp, "%d", current_webserver_port);
                    settings_set(settings, 1, "webserver.port", tmp);
                    new_webserver_port = current_webserver_port;
                }
            }
            free(key);
            if (val) free(val);

            // restart service if port number really changed
            if (new_webserver_port != current_webserver_port) {
                LDEBUG("webserver-monitor: port number changed from %d to %d, restarting webserver", current_webserver_port, new_webserver_port);
                zactor_destroy(&webserver_actor);
                webserver_actor = zactor_new(webserver_service, NULL);
                current_webserver_port = new_webserver_port;
            }
        } else if (sock == pipe) {
            break;
        }
    }

    LINFO("webserver-monitor: shutting down");
    if (webserver_actor) zactor_destroy(&webserver_actor);
    zsock_destroy(&settings);
    zsock_destroy(&port_changed);
    zpoller_destroy(&poller);
}

int init_webserver_service(void) {
    int bcast = 1, ret;
    bcast_sock = socket(AF_INET, SOCK_DGRAM, 0);
    // if bcast fails to initialize, be noisy, but it's not a fatal condition
    if (bcast_sock < 0) {
        LWARN("webserver: failed to initialize broadcast socket");
    } else {
        ret = setsockopt(bcast_sock, SOL_SOCKET, SO_BROADCAST, &bcast, sizeof(bcast));
        if (ret < 0) {
            LWARN("webserver: failed to set broadcast permissions on broadcast socket");
            shutdown(bcast_sock, 2);
            bcast_sock = -1;
        }
    }

    // if webserver can't start, then abort initialization. But if it can be
    // started, start the monitor. The monitor may destroy/restart the
    // service. This should be safe as long as we on the main thread never
    // directly touch it again. (This includes during shutdown.)
    webserver_actor = zactor_new(webserver_service, NULL);
    if (!webserver_actor) return 1;

    webserver_monitor = zactor_new(webserver_monitor_service, NULL);
    return 0;
}

void shutdown_webserver_service(void) {
    if (webserver_monitor) zactor_destroy(&webserver_monitor);

    // terminate bcast socket
    if (bcast_sock >= 0) {
        shutdown(bcast_sock, 2);
    } 
}
