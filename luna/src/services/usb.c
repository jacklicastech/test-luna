#define _GNU_SOURCE
#include "config.h"
#include <libgen.h>
#include "services.h"
#include "util/api_request.h"
#include "util/headers_parser.h"
#ifdef HAVE_CTOS
  #include <ctosapi.h>
#endif

// how many times per second to poll for rx data. Higher means worse
// battery life. FIXME can we select() on a device instead?
#define FREQUENCY 10

//static char __buf[1024];
//#define LINFO(...)  { sprintf(__buf, __VA_ARGS__); CTOS_LCDTPrint((BYTE *) __buf); CTOS_LCDTPrint((BYTE *) "\n"); }
//#define LWARN(...)  { sprintf(__buf, __VA_ARGS__); CTOS_LCDTPrint((BYTE *) __buf); CTOS_LCDTPrint((BYTE *) "\n"); }
//#define LERROR(...) { sprintf(__buf, __VA_ARGS__); CTOS_LCDTPrint((BYTE *) __buf); CTOS_LCDTPrint((BYTE *) "\n"); }
//#define LDEBUG(...) { sprintf(__buf, __VA_ARGS__); CTOS_LCDTPrint((BYTE *) __buf); CTOS_LCDTPrint((BYTE *) "\n"); }

static zactor_t *service = NULL;

#ifdef HAVE_CTOS
static char *process_request(const char *request_str, unsigned int request_size) {
  char verb[MAX_HTTP_VERB_LENGTH];
  char path[MAX_HTTP_PATH_LENGTH];
  unsigned int offset = scan_http_path(request_str, (unsigned) request_size, verb, path, MAX_HTTP_PATH_LENGTH);
  const char *request_headers_and_body = request_str + offset;
  LINFO("usb: Processing request: %s %s", verb, path);
  request_t *request = (request_t *) calloc(1, sizeof(request_t));
  assert(strlen(request_headers_and_body) == request_size - offset);
  offset += parse_headers(&(request->request_headers), request_headers_and_body, request_size - offset);
  char *request_body = strndup(request_str + offset, request_size - offset);
  assert(strlen(request_body) == request_size - offset);
  char *response_str = dispatch_request(verb, path, &(request->request_headers), request_body);
  free(request_body);
  return response_str;
}

static void usb_service(zsock_t *pipe, void *arg) {
  zpoller_t *poller = zpoller_new(pipe, NULL);
  USHORT res, bytes;
  DWORD status;
  int wait_ms = 1000 / FREQUENCY;
  char *input = (char *) calloc(1, sizeof(char));
  int shutting_down = 0;

  LINFO("usb: initializing service");
  
  res = CTOS_USBGetStatus(&status);
  if (res != d_OK) {
      LERROR("usb: failed to query USB status: %x", res);
      shutting_down = 1;
  } else {
      if (status & d_MK_USB_STATUS_CDCMODE) {
          LERROR("usb: running in CDC mode, aborting service initialization");
          shutting_down = 1;
      } else if (status & d_MK_USB_STATUS_HOSTMODE) {
          LERROR("usb: running in host mode, aborting service initialization");
          shutting_down = 1;
      }
  }
  
  zsock_signal(pipe, 0);
  if (shutting_down != -1) {
      if ((res = CTOS_USBOpen()) != d_OK) {
        LERROR("usb: error %x while opening connection", res);
        shutting_down = 1;
      }
  }
  
  
  while (!shutting_down) {
    void *in = zpoller_wait(poller, wait_ms);
    unsigned int input_size = 0;
    *input = '\0';

    if (!in) {
        if (!zpoller_expired(poller)) {
            LWARN("usb: service interrupted");
            break;
        }
    }

    if (in == pipe) {
      LDEBUG("usb: received shutdown signal");
      break;
    }

    while ((res = CTOS_USBRxReady(&bytes)) == d_OK && bytes > 0) {
//          if (_i == 0) { 
//              CTOS_LCDSelectModeEx(d_LCD_TEXT_MODE, d_TRUE);
//              CTOS_LCDTSelectFontSize(0x0A10);
//              _i = 1;
//          }
      input = (char *) realloc(input, input_size + bytes + 1);
      if ((res = CTOS_USBRxData((UCHAR *) (input + input_size), &bytes)) != d_OK) {
        LWARN("usb: rx: error %x while receiving", res);
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
          LERROR("usb: tx: error, took more than 5 seconds to transmit %d bytes", response_size);
          break;
        }

        if ((res = CTOS_USBTxReady()) != d_OK) {
          LWARN("usb: tx: error %x while checking whether tx is ok", res);
          continue;
        }

        if ((res = CTOS_USBTxData((UCHAR *) (response + tx_offset), tx_bytes)) == d_OK) {
          tx_offset += tx_bytes;
          response_size -= tx_bytes;
          LDEBUG("usb: tx: sent frame containing %d bytes", tx_bytes);
        } else {
            // just let it time out instead
//            LWARN("usb: tx: error %x while sending %d bytes", res, tx_bytes);
//            break;
        }
      }
      
      free(response);
    }
  }

  LINFO("usb: terminating service");
  free(input);
  CTOS_USBRxFlush();
  CTOS_USBClose();
  zpoller_destroy(&poller);
  LINFO("usb: shutdown complete");
}
#endif // HAVE_CTOS

int init_usb_service() {
#ifdef HAVE_CTOS
  service = zactor_new(usb_service, NULL);
  assert(service);
#else // HAVE_CTOS
  LWARN("usb: not supported on this device");
#endif // HAVE_CTOS
  return 0;
}

void shutdown_usb_service() {
  if (service) zactor_destroy(&service);
}
