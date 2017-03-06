#include "services.h"
#include "util/api_request.h"
#include "util/headers_parser.h"

#if HAVE_CTOS
#include <ctosapi.h>
#endif

// how many times per second to poll for rx data. Higher means worse
// battery life. FIXME can we select() on a device instead?
#define FREQUENCY 10

#define BLUETOOTH_EVENT_DELAY 30000 // send a bt event at most every 30 secs
#define BLUETOOTH_SERVICE_NAME "Appilee"
#define BLUETOOTH_SERVICE_UUID "f475dc12-d44f-11e5-9089-b3dacafc2697"

static zactor_t *service = NULL;

#ifdef HAVE_CTOS
  static char *process_request(const char *request_str, unsigned int request_size) {
    char verb[MAX_HTTP_VERB_LENGTH];
    char path[MAX_HTTP_PATH_LENGTH];
    unsigned int offset = scan_http_path(request_str, (unsigned) request_size, verb, path, MAX_HTTP_PATH_LENGTH);
    const char *request_headers_and_body = request_str + offset;
    LINFO("bluetooth: Processing request: %s %s", verb, path);
    request_t *request = (request_t *) calloc(1, sizeof(request_t));
    assert(strlen(request_headers_and_body) == request_size - offset);
    offset += parse_headers(&(request->request_headers), request_headers_and_body, request_size - offset);
    char *request_body = strndup(request_str + offset, request_size - offset);
    assert(strlen(request_body) == request_size - offset);
    char *response_str = dispatch_request(verb, path, &(request->request_headers), request_body);
    free(request_body);
    return response_str;
  }
  
  static void reflect_discoverability_setting(zsock_t *settings) {
    char *disc = NULL;
    settings_get(settings, 1, "bluetooth.discoverable", &disc);
    if (disc == NULL || !strcmp(disc, "")      ||
                        !strcmp(disc, "off")   ||
                        !strcmp(disc, "false") ||
                        !strcmp(disc, "disabled")) {
      LINFO("bluetooth: disabling discoverability");
      CTOS_BluetoothConfigSet(d_BLUETOOTH_CONFIG_DISCOVERABLE, (BYTE *) "0", 1);
    } else {
      LINFO("bluetooth: enabling discoverability");
      CTOS_BluetoothConfigSet(d_BLUETOOTH_CONFIG_DISCOVERABLE, (BYTE *) "1", 1);
    }
    if (disc) free(disc);
  }
  
  static void reflect_pin_setting(zsock_t *settings) {
    char *pin = NULL;
    settings_get(settings, 1, "bluetooth.pin", &pin);
    if (pin != NULL && strcmp(pin, "")) {
      LINFO("bluetooth: changing PIN");
      CTOS_BluetoothConfigSet(d_BLUETOOTH_CONFIG_PASSKEY, (BYTE *) pin, strlen(pin));
    } else {
      LINFO("bluetooth: generating a new PIN");
      if (pin) free(pin);
      pin = (char *) calloc(5, sizeof(char));
      for (int i = 0; i < 4; i++) {
        sprintf(pin + i, "%d", rand() % 10);
      }
      settings_set(settings, 1, "bluetooth.pin", pin);
    }
    if (pin) free(pin);
  }
#endif

static void bluetooth_service(zsock_t *pipe, void *arg) {
  #if HAVE_CTOS
    zsock_t *bluetooth_pub = zsock_new_pub(BLUETOOTH_ENDPOINT);
    zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
    zsock_t *setting_discoverable = zsock_new_sub(SETTINGS_CHANGED_ENDPOINT, "bluetooth.discoverable");
    zsock_t *setting_pin = zsock_new_sub(SETTINGS_CHANGED_ENDPOINT, "bluetooth.pin");
    zpoller_t *poller = zpoller_new(pipe, setting_discoverable, setting_pin, NULL);
    USHORT res, bytes;
    DWORD status;
    int wait_ms = 1000 / FREQUENCY;
    char *input = (char *) calloc(1, sizeof(char));
    int shutting_down = 0;
    unsigned long timer = 0;

    LINFO("bluetooth: initializing service");
    CTOS_BluetoothConfigSet(d_BLUETOOTH_CONFIG_SECURE, (BYTE *) "1", 1);
    CTOS_BluetoothSelectAccessMode(d_BLUETOOTH_SECURE_MODE_PASSKEYENTRY);
    zsock_signal(pipe, 0);
    
    if ((res = CTOS_BluetoothOpen()) != d_OK) {
      LERROR("bluetooth: error %x while opening connection", res);
      shutting_down = 1;
    }
    
    if (!shutting_down) {
      res = CTOS_BluetoothListen((BYTE *) BLUETOOTH_SERVICE_NAME, (BYTE *) BLUETOOTH_SERVICE_UUID);
      if (res != d_OK) {
        LWARN("bluetooth: %x while starting to listen", res);
        shutting_down = 1;
      }
    }
    
    if (!shutting_down) {
      reflect_discoverability_setting(settings);
      reflect_pin_setting(settings);
    }
    
    while (!shutting_down) {
      void *in = zpoller_wait(poller, wait_ms);
      unsigned int input_size = 0;
      *input = '\0';

      if (!in) {
          if (!zpoller_expired(poller)) {
              LWARN("bluetooth: service interrupted");
              break;
          }
      }

      if (in == pipe) {
        LDEBUG("bluetooth: received shutdown signal");
        break;
      }
      
      if (in == setting_discoverable) {
        zmsg_t *msg = zmsg_recv(setting_discoverable);
        zmsg_destroy(&msg);
        reflect_discoverability_setting(settings);
      }

      if (in == setting_pin) {
        zmsg_t *msg = zmsg_recv(setting_pin);
        zmsg_destroy(&msg);
        reflect_pin_setting(settings);
      }

      if (CTOS_TickGet() > timer) {
        timer = CTOS_TickGet() + (BLUETOOTH_EVENT_DELAY / 10);
        res = CTOS_BluetoothStatus(&status);

        if (res == d_OK) {
          zsock_send(bluetooth_pub, "sssssssssssssssss", "state-changed",
                     "connected",     status & d_BLUETOOTH_STATE_CONNECTED     ? "true" : "false",
                     "listening",     status & d_BLUETOOTH_STATE_LISTENING     ? "true" : "false",
                     "scanning",      status & d_BLUETOOTH_STATE_SCANNING      ? "true" : "false",
                     "connecting",    status & d_BLUETOOTH_STATE_CONNECTING    ? "true" : "false",
                     "sending",       status & d_BLUETOOTH_STATE_SENDING       ? "true" : "false",
                     "receiving",     status & d_BLUETOOTH_STATE_RECEIVING     ? "true" : "false",
                     "disconnecting", status & d_BLUETOOTH_STATE_DISCONNECTING ? "true" : "false",
                     "busy",          status & d_BLUETOOTH_STATE_BUSY          ? "true" : "false");
        } else {
          LWARN("bluetooth: could not query device status: error %x", res);
        }
      }

      while ((res = CTOS_BluetoothRxReady(&bytes)) == d_OK && bytes > 0) {
        input = (char *) realloc(input, input_size + bytes + 1);
        if ((res = CTOS_BluetoothRxData((UCHAR *) (input + input_size), &bytes)) != d_OK) {
          LWARN("bluetooth: rx: error %x while receiving", res);
          *input = '\0';
          break;
        }

        input_size += bytes;
        *(input + input_size) = '\0';
      }

      if (*input != '\0') {
        char *response = process_request(input, (unsigned int) input_size);
        int response_size = strlen(response);
        int tx_offset = 0;
        unsigned long timer_start = CTOS_TickGet();
        LDEBUG("response size: %d", response_size);

        while (response_size > 0) {
          LDEBUG("response size: %d", response_size);
          int tx_bytes = response_size > 2048 ? 2048 : (int) response_size;
          if (CTOS_TickGet() - timer_start > 500) {
            // took longer than 5 seconds to transmit, smth is wrong.
            LERROR("bluetooth: tx: error, took more than 5 seconds to transmit %d bytes", response_size);
            break;
          }

          if ((res = CTOS_BluetoothTxReady()) != d_OK) {
            LWARN("bluetooth: tx: error %x while checking whether tx is ok", res);
            continue;
          }

          res = CTOS_BluetoothTxData((UCHAR *) (response + tx_offset), tx_bytes);
          while (res == d_BLUETOOTH_IO_PROCESSING) {
              if (CTOS_TickGet() - timer_start > 500) {
                  LERROR("bluetooth: tx: timeout while IO processing");
                  break;
              }
              res = CTOS_BluetoothStatus(&status);
          }
          
          if (res == d_OK) {
            tx_offset += tx_bytes;
            response_size -= tx_bytes;
            LDEBUG("bluetooth: tx: sent frame containing %d bytes", tx_bytes);
          } else {
            // just let it time out instead
            LWARN("bluetooth: tx: error %x while sending %d bytes", res, tx_bytes);
//            break;
          }
        }
        
        free(response);
      }
    }

    LINFO("bluetooth: terminating service");
    free(input);
    CTOS_BluetoothDisconnect();
    CTOS_BluetoothClose();
    zsock_destroy(&bluetooth_pub);
    zsock_destroy(&settings);
    zsock_destroy(&setting_discoverable);
    zsock_destroy(&setting_pin);
    zpoller_destroy(&poller);
    LINFO("bluetooth: shutdown complete");
  #else // HAVE_CTOS
    LWARN("bluetooth: not supported on this device");
  #endif // HAVE_CTOS
}

int init_bluetooth_service() {
  service = zactor_new(bluetooth_service, NULL);
  assert(service);
  return 0;
}

void shutdown_bluetooth_service() {
  zactor_destroy(&service);
}
