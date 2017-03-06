/**
 * Performs PIN entry. No UI operations are performed. Receive messages from
 * this service by connecting to the "inproc://events/sub" endpoint. Send
 * messages to this service by sending them to the "inproc://events/pub"
 * endpoint.
 * 
 *
 * The following types of socket message may originate on the service side:
 *
 * 1. PIN entry started: Published when the "start" message has been received.
 *    Sends the following frames:
 *
 *        ["pin-entry", "started"]
 *
 *
 * 2. PIN character added: Published when the user presses a button other than
 *    the "backspace" button (or equivalent). Sends the following frames:
 *
 *        ["pin-entry", "key-added", "[appended]", "[length]"]
 * 
 *    The third frame contains "true" if the pressed key was added to the PIN,
 *    or "false" if the key was not added to the PIN (e.g. the PIN is already
 *    at the maximum length).
 *    
 *    The fourth frame contains the length of the unencrypted PIN, encoded as
 *    a string containing a base-10 number, after the new key press has been
 *    processed.
 *
 *
 * 3. PIN character removed: Published when the user presses a button
 *    equivalent to "backspace". Sends the following frames:
 *
 *        ["pin-entry", "key-removed", "[removed]", "[length]"]
 *
 *    The third frame contains "true" if the last key was removed from the
 *    PIN, or "false" if it was not removed (e.g. because the PIN already has
 *    length 0). If the third frame is "true", the removed character is always
 *    the final character in the PIN.
 *
 *    The fourth frame contains the length of the unencrypted PIN, encoded as
 *    a string containing a base-10 number, after the backspace has been
 *    processed.
 *
 *
 * 4. PIN entry complete: Published when the user presses the "Enter" (or
 *    equivalent) button on the keypad. This message will be sent even if the
 *    PIN is blank. Sends the following frames:
 *
 *        ["pin-entry", "complete", "[length]", "[pin block]", "[KSN]"]
 *
 *    The third frame contains the length of the unencrypted PIN, encoded as a
 *    string containing a base-10 number.
 *
 *    The fourth frame contains the token ID of the encrypted PIN block.
 *
 *    The fifth frame contains the token ID of the KSN.
 *
 *
 * 5. PIN entry cancelled: Published when the user presses the "Cancel" (or
 *    equivalent) button on the keypad. Sends the following frames:
 *
 *        ["pin-entry", "cancelled"]
 *
 * 6. PIN entry error: if any error occurs during PIN entry, this message will
 *    be sent. Sends the following frames:
 *
 *        ["pin-entry", "error", "[errno]", "[errmsg]", "[cancelled]"]
 *
 *    The third frame contains the error number of the error, encoded as a
 *    string containing a base-10 number.
 *
 *    The fourth frame contains a human-readable error message.
 *
 *    The fifth frame contains "true" if the error was fatal and the PIN entry
 *    attempt was aborted, "false" if the error was non-fatal and the PIN
 *    entry attempt is still ongoing.
 *
 * 
 * The following types of socket message may originate on the client side:
 *
 * 1. Start: Sent when the consuming party wishes to begin PIN entry. If the
 *    keypad service is running, it will be suspended until PIN entry has been
 *    completed, aborted or cancelled. If the keypad service was running prior
 *    to PIN entry, it will be resumed. If this message is received while PIN
 *    entry is already in progress, a warning will be logged and the message
 *    will be discarded. Send the following frames:
 *
 *        ["pin-entry", "start", "[pan-token]", "[min-size]", "[max-size]",
 *         "[timeout]", "[type]"]
 *
 *    The third frame contains the PAN or the token ID containing the PAN.
 *
 *    The fourth frame represents the minimum length of the unencrypted PIN in
 *    characters. It should be encoded as a string containing a base-10
 *    number.
 *
 *    The fifth frame represents the maximum length of the unencrypted PIN in
 *    characters. It should be encoded as a string containing a base-10
 *    number. It may not exceed 255. If it is too large, an error will be
 *    emitted immediately and PIN entry will not take place.
 *
 *    The sixth frame represents the amount of time PIN entry can take, in
 *    seconds, before timing out. It should be encoded as a string containing
 *    a base-10 number. Specify 0 to indicate either no timeout or the maximum
 *    timeout this device will allow.
 *
 *    The seventh frame contains the value "online" or "offline". The
 *    appropriate cipher will be chosen depending upon this value.
 *
 **/

#include "config.h"
#include <czmq.h>
#include "plugin.h"
#include "util/detokenize_template.h"
#include "util/string_helpers.h"

#if HAVE_CTOS
  #include <ctosapi.h>
#endif

void process_message(zmsg_t *msg);

#define KEYPAD_POLL_MS          150 // check for input every 150ms
#define d_ONLINE_KEYSET         0xC001
#define d_ONLINE_KEYINDEX       0x001A

// Platform-independent error codes
#define OK                      0x00
#define NOT_SUPPORTED           0x01
#define INVALID_PARA            0x02
#define FAILED                  0x03
#define SYSTEM_ERROR            0x04
#define NOT_OWNER               0x05
#define KEY_NOT_EXIST           0x06
#define KEYTYPE_INCORRECT       0x07
#define KEY_NOT_ALLOWED         0x08
#define KEY_VERIFY_INCORRECT    0x09
#define CERT_INCORRECT          0x0B
#define HASH_INCORRECT          0x0C
#define CERT_PARA_INCORRECT     0x0D
#define INSUFFICIENT_BUFFER     0x0E
#define DUKPT_KEY_NOT_GENERATED 0x0F
#define PIN_ABORTED             0x10
#define PIN_TIMEOUT             0x11
#define NULL_PIN                0x12
#define PKCS_FORMAT_ERROR       0x13
#define KEY_VALUE_NOT_UNIQUE    0x14
#define KEY_TYPE_MISMATCH       0x15
#define DUKPT_KEY_EXPIRED       0x16
#define PURPOSE_NOT_UNIQUE      0x17
#define FUNCTION_IS_FORBIDDEN   0x18
#define FTOK_FAILURE            0x1A
#define SHMGET_FAILURE          0x1B
#define SHMAT_FAILURE           0x1E
#define IPC_FAILURE             0x1F
#define ILLEGAL_PATH            0x20
#define UNKNOWN                 0x21
#define INCOMPLETE_MESSAGE      0x22
#define TOO_SHORT               0x23

typedef struct {
  bool in_progress;
  char pan[256];
  int  min_size;
  int  max_size; // not greater than 255
  int  current_size;
  bool keypad_service_was_running;
  int  timeout;
} pindata_t;

typedef struct { int plt_code; int normalized_code; const char *msg; } error_t;

static const error_t ERRORS[] = {
#ifdef HAVE_CTOS
  { d_KMS2_INVALID_PARA,               INVALID_PARA,            "invalid parameter" },
  { d_KMS2_FAILED,                     FAILED,                  "failure" },
  { d_KMS2_SYSTEM_ERROR,               SYSTEM_ERROR,            "system error" },
  { d_KMS2_NOT_OWNER,                  NOT_OWNER,               "not owner" },
  { d_KMS2_KEY_NOT_EXIST,              KEY_NOT_EXIST,           "key does not exist" },
  { d_KMS2_KEYTYPE_INCORRECT,          KEYTYPE_INCORRECT,       "key type incorrect" },
  { d_KMS2_KEY_NOT_ALLOWED,            KEY_NOT_ALLOWED,         "key not allowed" },
  { d_KMS2_KEY_VERIFY_INCORRECT,       KEY_VERIFY_INCORRECT,    "key verification failed" },
  { d_KMS2_NOT_SUPPORTED,              NOT_SUPPORTED,           "not supported" },
  { d_KMS2_CERTIFICATE_INCORRECT,      CERT_INCORRECT,          "cert incorrect" },
  { d_KMS2_HASH_INCORRECT,             HASH_INCORRECT,          "hash incorrect" },
  { d_KMS2_CERTIFICATE_PARA_INCORRECT, CERT_PARA_INCORRECT,     "cert parameter incorrect" },
  { d_KMS2_INSUFFICIENT_BUFFER,        INSUFFICIENT_BUFFER,     "insufficient buffer" },
  { d_KMS2_DUKPT_KEY_NOT_GENERATED,    DUKPT_KEY_NOT_GENERATED, "unique key not generated" },
  { d_KMS2_GET_PIN_ABORT,              PIN_ABORTED,             "PIN aborted" },
  { d_KMS2_GET_PIN_TIMEOUT,            PIN_TIMEOUT,             "PIN timeout" },
  { d_KMS2_GET_PIN_NULL_PIN,           NULL_PIN,                "NULL PIN" },
  { d_KMS2_PKCS_FORMAT_ERROR,          PKCS_FORMAT_ERROR,       "PKCS format error" },
  { d_KMS2_KEY_VALUE_NOT_UNIQUE,       KEY_VALUE_NOT_UNIQUE,    "key value not unique" },
  { d_KMS2_KEY_TYPE_NOT_MATCH,         KEY_TYPE_MISMATCH,       "key type mismatch" },
  { d_KMS2_DUKPT_KEY_EXPIRED,          DUKPT_KEY_EXPIRED,       "key expired" },
  { d_KMS2_PURPOSE_NOT_UNIQUE,         PURPOSE_NOT_UNIQUE,      "purpose not unique" },
  { d_KMS2_FUNCTION_IS_FORBIDDEN,      FUNCTION_IS_FORBIDDEN,   "function is forbidden" },
  { d_KMS2_ERR_UNSUPPORTED,            NOT_SUPPORTED,           "not supported" },
  { d_KMS2_ERR_FTOK_FAIL,              FTOK_FAILURE,            "FTOK failure" },
  { d_KMS2_ERR_SHMGET_FAIL,            SHMGET_FAILURE,          "SHMGET failure" },
  { d_KMS2_ERR_SHMAT_FAIL,             SHMAT_FAILURE,           "SHMAT failure" },
  { d_KMS2_ERR_IPC_FAIL,               IPC_FAILURE,             "IPC failure" },
  { d_KMS2_ERR_PATH_ILLEGAL,           ILLEGAL_PATH,            "illegal KMS path" },
#else
  // Generic errors (we don't know what platform we're on)
  { NOT_SUPPORTED,                     NOT_SUPPORTED,           "not supported" },
#endif
  { TOO_SHORT,                         TOO_SHORT,               "too short" },
  { INCOMPLETE_MESSAGE,                INCOMPLETE_MESSAGE,      "incomplete message" },
  { UNKNOWN,                           UNKNOWN,                 "unknown" },
  { OK,                                OK,                      "OK" },
  { OK,                                OK,                      NULL }
};

static bool      first_init = false;
static zactor_t  *service   = NULL;
static zsock_t   *sub       = NULL;
static zsock_t   *pub       = NULL;
static zpoller_t *poller    = NULL;
static pindata_t pin;
static bool      service_running = false;

/*
 * Translates a platform-specific error code into 'luna' error code.
 */
int translate_errno(int plt_code) {
  int i;
  for (i = 0; true; i++) {
    if (ERRORS[i].msg == NULL)
      break;
    if (ERRORS[i].plt_code == plt_code)
      return ERRORS[i].normalized_code;
  }
  LWARN("pin-entry: BUG: no translation for error code: %x", plt_code);
  return UNKNOWN;
}

/*
 * Returns the human-readable string representation of the specified 'luna'
 * error code.
 */
const char *pinentry_strerror(int normalized_code) {
  int i;
  for (i = 0; true; i++) {
    if (ERRORS[i].msg == NULL)
      break;
    if (ERRORS[i].normalized_code == normalized_code)
      return ERRORS[i].msg;
  }
  LWARN("pin-entry: BUG: no text for error code: %x", normalized_code);
  return "unknown";
}

/*
 * Stores the next KSN value into `*out`. If an error occurs, `*out` is not
 * modified. Returns `OK` on success, otherwise a normalized error code
 * is returned.
 *
 * If successful, the value in `*out` must be freed.
 */
int get_ksn(char **out) {
#if HAVE_CTOS
  BYTE length = 10;
  BYTE ksnbuf[length];
  int res = CTOS_KMS2DUKPTGetKSN(d_ONLINE_KEYSET, d_ONLINE_KEYINDEX, ksnbuf, &length);
  if(res != d_OK) {
    res = translate_errno(res);
    const char *msg = pinentry_strerror(res);
    LWARN("emv: contact: unable to get KSN for online PIN entry: %x (%s)", res, msg);
    return res;
  } else {
    *out = bytes2hex((char *) ksnbuf, length);
    return OK;
  }
#else
  return NOT_SUPPORTED;
#endif
}

int check_key(int keyset, int index) {
#ifdef HAVE_CTOS
  int res = CTOS_KMS2KeyCheck(d_ONLINE_KEYSET, d_ONLINE_KEYINDEX);
#else
  (void) keyset;
  (void) index;
  int res = NOT_SUPPORTED;
#endif
  res = translate_errno(res);
  return res;
}

void start_keypad_service(void) {
  plugin_t *plugin = find_plugin_by_name("keypad");
  if (plugin && plugin->service.init) {
    int err = plugin->service.init(0, NULL);
    if (err) {
      LERROR("pin-entry: couldn't start keypad service: it failed with error %d", err);
    }
  } else {
    LWARN("pin-entry: can't start keypad service");
  }
}

void reset_pin_entry(void) {
  if (pin.keypad_service_was_running)
    start_keypad_service();
  memset(&pin, 0, sizeof(pindata_t));
}

/*
 * Diagnostic on the injected key key; has no effect on this service because
 * we need to e.g. publish errors if PIN entry is attempted anyway, but we
 * should at least log known infos to aid in debugging. Returns `OK` on
 * success.
 */
int perform_key_diagnostic(void) {
  int err = check_key(d_ONLINE_KEYSET, d_ONLINE_KEYINDEX);
  if (err == OK) {
    LINFO("pin-entry: KMS key check OK");
  } else {
    LERROR("pin-entry: KMS key check failed: %s (%x)", pinentry_strerror(err), err);
    if (err != NOT_SUPPORTED)
      LERROR("pin-entry: key might not have been injected");
  }
  return err;
}

void publish_error(int err, bool fatal) {
  const char *fatal_str = "false";
  if (fatal) fatal_str = "true";
  zsock_send(pub, "ssiss", "pin-entry", "error", err, pinentry_strerror(err), fatal_str);
}

bool stop_keypad_service(void) {
  plugin_t *plugin = find_plugin_by_name("keypad");
  if (plugin && plugin->service.is_running) {
    pin.keypad_service_was_running = plugin->service.is_running();
    if (pin.keypad_service_was_running) {
      if (plugin->service.shutdown) {
        LTRACE("pin-entry: stopping keypad service");
        plugin->service.shutdown();
        return true;
      } else {
        LWARN("pin-entry: can't stop keypad service");
      }
    } else {
      LTRACE("pin-entry: keypad service is not running");
    }
  } else {
    LWARN("pin-entry: can't query whether keypad service is running; won't try to stop it");
  }

  return false;
}

int poll_for_messages(int timeout) {
  void *in = zpoller_wait(poller, timeout);
  if (in == NULL) {
    if (!zpoller_expired(poller)) {
      LWARN("pin-entry: interrupted!");
      return 1;
    }
  } else if (in == sub) {
    zmsg_t *msg = zmsg_recv(sub);
    process_message(msg);
    zmsg_destroy(&msg);
  } else { // in == actor pipe
    LDEBUG("pin-entry: shutdown signal received");
    service_running = false;
    return 2;
  }

  return 0;
}

// CTOS PIN entry callbacks
#if HAVE_CTOS
  // int ctos_poll_for_abort(void) {
  //   if (poll_for_messages(0)) return 1;
  //   // after processing messages, if anything happens to set in_progress to
  //   // false, stop processing the txn. Really only an 'abort' message can bring
  //   // us into this state.
  //   if (!pin.in_progress) return 1;
  //   return 0;
  // }

  void ctos_pin_digit_received(BYTE num_digits) {
    const char *size_changed = "false";
    if (pin.current_size + 1 == num_digits) size_changed = "true";
    pin.current_size = num_digits;
    zsock_send(pub, "sssi", "pin-entry", "key-added", "true", num_digits);
  }

  void ctos_cancel_received(void) {
    zsock_send(pub, "ss", "pin-entry", "cancelled");
  }

  void ctos_backspace_received(BYTE num_digits) {
    const char *size_changed = "false";
    if (pin.current_size - 1 == num_digits) size_changed = "true";
    pin.current_size = num_digits;
    zsock_send(pub, "sssi", "pin-entry", "key-removed", "true", num_digits);
  }
#endif

int pin_start(zmsg_t *msg) {
  if (pin.in_progress) {
    LWARN("pin-entry: already in progress");
    return OK;
  }

  char *pan          = zmsg_popstr(msg);
  char *min_size_str = zmsg_popstr(msg);
  char *max_size_str = zmsg_popstr(msg);
  char *timeout_str  = zmsg_popstr(msg);
  char *type_str     = zmsg_popstr(msg);
  if (!pan || !min_size_str || !max_size_str || !timeout_str || !type_str) {
    if (pan)          free(pan);
    if (min_size_str) free(min_size_str);
    if (max_size_str) free(max_size_str);
    if (timeout_str)  free(timeout_str);
    return INCOMPLETE_MESSAGE;
  }

  pin.min_size = atoi(min_size_str);
  pin.max_size = atoi(max_size_str);
  pin.timeout  = atoi(timeout_str);
  free(min_size_str);
  free(max_size_str);
  free(timeout_str);
  int cipher = 0;
  #define CIPHER_OFFLINE 1
  #define CIPHER_ONLINE  2
  if (!strcmp(type_str, "offline")) {
    LDEBUG("pin-entry: using offline cipher");
    cipher = CIPHER_OFFLINE;
  } else if (!strcmp(type_str, "online")) {
    LDEBUG("pin-entry: using online cipher");
    cipher = CIPHER_ONLINE;
  } else {
    LWARN("pin-entry: unexpected type: %s (should be 'online' or 'offline')", type_str);
    LWARN("pin-entry: using online cipher");
    cipher = CIPHER_ONLINE;
  }
  if (pin.max_size > 255) return INVALID_PARA;
  else pin.in_progress = true;

  size_t tok_len = strlen(pan);
  char *detokenized_pan = detokenize_template(pan, &tok_len);
  assert(detokenized_pan != NULL);
  strncpy(pin.pan, detokenized_pan, sizeof(pin.pan) - 1);
  free(detokenized_pan);
  free(pan);

  // // Work around a castles bug which does not drop the check digit for AmEx
  // // cards, because they are shorter than others. The PAN prefix check
  // // indicates whether this is AmEx, and the length == 15 check hopefully
  // // future-proofs against any bug fixes, as the length should be shorter
  // // after the fix.
  // if (strlen(pin.pan) == 15 && (!strncmp(pin.pan, "37", 2) ||
  //                               !strncmp(pin.pan, "34", 2)))
  //   *(pin.pan + 14) = '\0';
  LINSEC("Generating PIN block using PAN (%d bytes): %s", (int) strlen(pin.pan), pin.pan);

  pin.keypad_service_was_running = stop_keypad_service();
  assert(cipher != 0);

#if HAVE_CTOS
  switch(cipher) {
    case CIPHER_OFFLINE: cipher = KMS2_PINCIPHERMETHOD_EMV_OFFLINE_PIN; break;
    case CIPHER_ONLINE:  cipher = KMS2_PINCIPHERMETHOD_ECB;             break;
  }

  zsock_send(pub, "ss", "pin-entry", "started");
  CTOS_KMS2PINGET_PARA_VERSION_2 para;
  BYTE pin_block[32];
  memset(&para, 0, sizeof(CTOS_KMS2PINGET_PARA_VERSION_2));
  para.Version = 0x02;
  para.PIN_Info.BlockType             = KMS2_PINBLOCKTYPE_ANSI_X9_8_ISO_0;
  para.PIN_Info.PINDigitMinLength     = pin.min_size;
  para.PIN_Info.PINDigitMaxLength     = pin.max_size;
  para.Protection.CipherKeySet        = d_ONLINE_KEYSET;
  para.Protection.CipherKeyIndex      = d_ONLINE_KEYINDEX;
  para.Protection.CipherMethod        = cipher;
  para.Protection.SK_Length           = 0;
  para.AdditionalData.InLength        = strlen(pin.pan);
  para.AdditionalData.pInData         = (BYTE *) pin.pan;
  para.PINOutput.EncryptedBlockLength = 16;
  para.PINOutput.pEncryptedBlock      = pin_block;
  para.Control.Timeout                = (DWORD) pin.timeout;
  para.Control.NULLPIN                = FALSE;
  // para.Control.piTestCancel           = ctos_poll_for_abort;
  para.EventFunction.OnGetPINDigit    = ctos_pin_digit_received;
  para.EventFunction.OnGetPINCancel   = ctos_cancel_received;
  para.EventFunction.OnGetPINBackspace= ctos_backspace_received;
  char *ksn = NULL;
  int res = get_ksn(&ksn);
  if (res != OK) {
    publish_error(res, true);
    return OK;
  }
  res = translate_errno(CTOS_KMS2PINGet((CTOS_KMS2PINGET_PARA *) &para));
  start_keypad_service();
  reset_pin_entry();
  if (res == OK) {
    // ["pin-entry", "complete", "[length]", "[pin block]", "[KSN]"]
    char *hex_pin_block = bytes2hex((char *) pin_block, para.PINOutput.EncryptedBlockLength);
    zsock_send(pub, "ssiss", "pin-entry", "complete", para.PINOutput.PINDigitActualLength,
                             hex_pin_block, ksn);
    free(hex_pin_block);
    free(ksn);
  } else {
    publish_error(res, true);
    return OK;
  }
#else
  publish_error(NOT_SUPPORTED, true);
  reset_pin_entry();
#endif
  return OK;
}

// int pin_abort(zmsg_t *msg) {
//   if (!pin.in_progress) {
//     LWARN("pin-entry: not in progress, nothing to abort");
//     return OK;
//   }

//   reset_pin_entry();
//   zsock_send(pub, "ss", "pin-entry", "aborted");
//   return OK;
// }

void process_message(zmsg_t *msg) {
  int err = OK;
  int fatal = true;
  char *channel  = zmsg_popstr(msg);
  char *topic    = zmsg_popstr(msg);
  if (channel) free(channel); // we already got the message
  if (!topic) {
    err = INCOMPLETE_MESSAGE;
    fatal = false; // if message can't be processed, we can't abort.
  } else if (!strcmp(topic, "start")) {
    err = pin_start(msg);
    fatal = true;  // if we fail to start, then we definitely didn't start.
  // } else if (!strcmp(topic, "abort")) {
  //   err = pin_abort(msg);
  //   fatal = false; // if we fail to abort, then we haven't aborted, right?
  } else {
    // ignore messages from this service
    if (strcmp(topic, "started") &&
        strcmp(topic, "complete") &&
        strcmp(topic, "error") &&
        strcmp(topic, "aborted") &&
        strcmp(topic, "cancelled") &&
        strcmp(topic, "key-added") &&
        strcmp(topic, "key-removed")) {
      LWARN("pin-entry: unrecognized topic: %s", topic);
      fatal = false; // if message can't be processed, we can't abort.
      err = INVALID_PARA;
    }
  }

  if (err != OK) publish_error(err, fatal);
  if (topic) free(topic);
}

void pin_entry_service(zsock_t *pipe, void *arg) {
  (void) arg;

  sub    = zsock_new_sub(">" EVENTS_SUB_ENDPOINT, "pin-entry");
  assert(sub);
  
  pub    = zsock_new_pub(">" EVENTS_PUB_ENDPOINT);
  assert(pub);
  
  poller = zpoller_new(pipe, sub, NULL);
  assert(poller);

  memset(&pin, 0, sizeof(pin));

  if (perform_key_diagnostic())
    LERROR("pin-entry: will probably fail when PIN entry is attempted");
  zsock_signal(pipe, OK);

  service_running = true;
  while (service_running) {
    // try to wait forever, unless we are also waiting for keypad
    int timeout = -1;
    if (pin.in_progress) timeout = KEYPAD_POLL_MS;
    if (poll_for_messages(timeout)) break;
  }

  LINFO("pin-entry: shutting down");
  zpoller_destroy(&poller);
  zsock_destroy(&pub);
  zsock_destroy(&sub);
  poller = NULL;
  pub = sub = NULL;
  reset_pin_entry();
  LDEBUG("pin-entry: shutdown complete");
}

int init_pin_entry_service(int argc, const char *argv[]) {
  (void) get_ksn;

  if (!first_init) {
    #if HAVE_CTOS
      CTOS_KMS2Init();
    #endif
    first_init = true;
  }

  if (service) {
    LWARN("pin-entry: service already running");
  } else {
    service = zactor_new(pin_entry_service, NULL);
    LINFO("pin-entry: service started");
  }
  return 0;
}

void shutdown_pin_entry_service(void) {
  if (service) {
    zactor_destroy(&service);
    service = NULL;
    LINFO("pin-entry: shutdown complete");
  } else {
    LWARN("pin-entry: not running, nothing to terminate");
  }
}

bool is_pin_entry_service_running(void) { return !!service; }

// LUALIB_API int init_pin_entry_lua(lua_State *L) {
//   LINFO("hello from pin entry lua");
//   return 0;
// }

// void shutdown_pin_entry_lua(lua_State *L) {
//   (void) L;
//   LINFO("pin entry lua shutting down");
// }
