/**
 * Performs contactless EMV transactions. No UI operations are performed.
 * Receive messages from this service by connecting to the
 * "inproc://events/sub" endpoint. Send messages to this service by sending
 * them to the "inproc://events/pub" endpoint.
 * 
 *
 * The following types of socket message may originate on the service side:
 *
 * 1. Error: Published when an EMV processing error occurs. Sends the
 *    following frames:
 *
 *        ["contactless-emv", "error", "[errno]", "[message]"]
 *
 *    The third frame contains a number identifying the error, encoded as
 *    a string containing a base-10 number, after the backspace has been
 *    processed.
 *
 *    The fourth frame contains a human-readable text message explaining the
 *    error.
 *
 *
 * 2. Starting transaction: Published when transaction processing has begun.
 *    Sends the following frames:
 *
 *        ["contactless-emv", "txn-started", "[attempts-remaining]"]
 *
 *    The third frame contains the number of attempts remaining, encoded as a
 *    string containing a base-10 number. This number includes the current
 *    attempt, so it will never be less than 1.
 *
 *
 * 3. Message: Published when the EMV kernel decides a message should be shown
 *    to the end user. Sends the following frames:
 *
 *        ["contactless-emv", "message", "[type]", "[text]"]
 *
 *    The third frame contains either "error" or "info", indicating the type
 *    of message to be displayed.
 *
 *    The fourth frame contains the human-readable text of the message to
 *    display. Note that the language can change depending on the EMV card
 *    being used.
 *
 *
 * 4. PIN verified offline: Published when the EMV kernel performs PIN
 *    verification offline. Not sent if PIN verification will occur online.
 *    Sends the following frames:
 *
 *        ["contactless-emv", "pin-verified-offline", "[result]"]
 *
 *    The third frame contains one of the values "ok", "failed", "blocked" or
 *    "failed+blocked".
 *
 *
 * 5. Issuer script result: Published when an issuer script has finished
 *    processing and a result is ready. Sends the following frames:
 *
 *        ["contactless-emv", "issuer-script-result", "[hex]"]
 *
 *    The third frame is encoded as a string containing a hexadecimal value,
 *    which was received from the EMV kernel.
 *
 *
 * 6. Transaction complete: Published at the end of a transaction. No further
 *    events for this transaction will be published. Sends the following
 *    frames:
 *
 *        ["contactless-emv", "txn-complete", "[result]", "[signature-required]",
 *         "[emv-tags]", "[pan-seq-number]", "[track2]", "[online]",
 *         "[contactless]", "[card-type]"]
 *
 *    The third frame contains one of the values "approved", "declined" or
 *    "unknown". Note that the value "unknown" indicates a bug in the EMV
 *    service, and shouldn't be an expected result in practice.
 *
 *    The fourth frame contains "true" or "false", and indicates whether an
 *    end user's signature is required for the completion of this transaction.
 *
 *    The fifth frame contains EMV tags which can be readily parsed. This
 *    value is encoded as a string containing hexadecimal characters and does
 *    not contain sensitive data.
 *
 *    The sixth frame contains the PAN sequence number, or the value "-1" if
 *    no PAN sequence number is available for this card. It is encoded as a
 *    string containing a base-10 number.
 *
 *    The seventh frame contains a token ID referring to track 2 data.
 *
 *    The eighth frame contains "true" if this transaction was processed
 *    online, "false" otherwise. This is sent as a convenience: if this event
 *    was received and no "run transaction online" (#11) event was received,
 *    this transaction can safely be assumed to be an offline transaction.
 *
 *    The ninth frame contains "true" to signal that this was a contactless
 *    transaction.
 *
 *    The tenth frame contains "EMV" or "MSD" to indicate the type of
 *    contactless card.
 *
 *
 * 7. Ready for transaction data: Published when the EMV kernel is prepared to
 *    receive transaction data such as amount, transaction type, etc. Sends
 *    the following frames:
 *
 *        ["contactless-emv", "ready-for-txn-data"]
 *
 * 
 * 8. Ready for app selection: Published when the EMV kernel has detected
 *    multiple applications on the card and must be instructed as to which
 *    app should be used. Sends the following frames:
 *
 *        ["contactless-emv", "ready-for-app-selection", "[app-name]", ...]
 *
 *    The third and subsequent frames contain human-readable application
 *    names, which can be presented to an end user for selection. When
 *    sending the response to this message, the third frame (which is the
 *    first application) should be treated as index 0.
 *
 *
 * 9. Single app has been selected: Published when the EMV kernel has
 *    detected only one viable application on the card, and would like to
 *    automatically select it. A confirmation may need to be shown to the end
 *    user to allow them a chance to abort the transaction. Sends the
 *    following frames:
 *
 *        ["contactless-emv", "single-app-selected", "[app-name]",
 *         "[must-confirm]"]
 *
 *    The third frame contains a human-readable application name, which can
 *    be shown to the end user.
 *
 *    The fourth frame contains "true" if the EMV kernel has indicated that
 *    end-user confirmation is required in order to proceed; "false"
 *    otherwise.
 *
 * 
 * 10. Performing PIN entry: Published when PIN entry is required. You must
 *     send a "pin-entry-ui" message describing the UI for PIN entry. Sends
 *     the following frames:
 *
 *         ["contactless-emv", "performing-pin-entry", "[remaining_count]",
 *          "[type]"]
 *
 *     The third frame contains the number of PIN entry attempts remaining.
 *
 *     The fourth frame contains the value "online" or "offline", representing
 *     whether the PIN will be verified online or offline.
 *
 *
 * 11. Run transaction online: Published when the transaction must be
 *     completed online. Online processing must be performed and the result
 *     must be sent to this service before the transaction can be completed.
 *     Sends the following frames:
 *
 *         ["contactless-emv", "online", "[pin-block]", "[ksn]", "[emv-data]",
 *          "[pan-seq-number]", "[track2]", "[contactless]", "[card-type]",
 *          "[scheme]"]
 *
 *     The third frame contains the encrypted PIN block that was sent to this
 *     service following PIN entry. It may be blank if PIN verification was
 *     performed offline.
 *
 *     The fourth frame contains the KSN that was sent to this service
 *     following PIN entry. It may be blank if PIN verification was performed
 *     offline.
 * 
 *     The fifth frame contains a token ID referencing the EMV data, some of
 *     which contains sensitive information.
 *
 *     The sixth frame contains the PAN sequence number, encoded as a string
 *     containing a base-10 number, or "-1" if the PAN sequence number is not
 *     available for this transaction.
 *
 *     The seventh frame contains a token ID referencing the track 2
 *     equivalent. It is considered sensitive data.
 *
 *     The eigth frame contains "true" to signal that this was a contactless
 *     transaction.
 *
 *     The ninth frame contains "EMV" or "MSD" to indicate the type of
 *     contactless card.
 *
 *     The eleventh frame contains the card scheme.
 *
 *
 * The following types of socket message may originate on the client side:
 *
 * 1. Continue: Sent to indicate that the transaction may proceed. The
 *    following message types will stop transaction processing while waiting
 *    for either this message or an Abort (#2) message:
 *
 *        error
 *        message
 *        txn-started
 *        pin-verified-offline
 *        issuer-script-result
 *        single-app-selected
 * 
 *    Send the following frames:
 *
 *        ["contactless-emv", "continue"]
 *
 *
 * 2. Abort: Sent to indicate that the transaction should be aborted at the
 *    next opportunity. This message can be sent at any time and will result
 *    in an "error" event representing the fact that the transaction was
 *    aborted. Send the following frames:
 *
 *        ["contactless-emv", "abort"]
 *
 * 
 * 3. Select app: When multiple applications are available, and a
 *    "ready-for-app-selection" event has been published, the service will
 *    block until either this message or an Abort (#2) is published. Use this
 *    message to choose one of the available applications. Send the following
 *    frames:
 *
 *        ["contactless-emv", "select-app-index", "[index]"]
 *
 *    The third frame is the index of the application to select, with index 0
 *    being the first application, encoded as a string containing a base-10
 *    number. If the index is out of bounds, an "error" will be published and
 *    the transaction will be aborted.
 *
 * 
 * 4. Transaction data: After the "ready-for-txn-data" event has been
 *    published, send this message to specify the transaction data, or send
 *    an Abort (#2) to cancel the transaction. Send the following frames:
 *
 *        ["contactless-emv", "txn-data", "[type]", "[force-online]",
 *         ["txn-amount"]]
 *
 *    The third frame must contain "sale", "balance-inqury", "refund",
 *    "return", or "verification". Note that the values "refund" and "return"
 *    are equivalent.
 *
 *    The fourth frame must contain "true" to force the transaction to run
 *    online, or "false" to let the EMV kernel decide.
 *
 *    The fifth frame must contain the transaction amount encoded as
 *    a string containing a base-10 integer value (no decimals). Example:
 *    "100" for $1.00.
 *
 *
 * 5. PIN UI info: After the "performing-pin-entry" event has been published,
 *    send this message to indicate the UI and other information that will be
 *    used to perform PIN entry. Send the following frames:
 *
 *        ["contactless-emv", "pin-entry-ui", "[left]", "[right]", "[top]",
 *         "[timeout]", "[min_size]", "[max_size]"]
 *
 *    The third frame contains the distance of the left side of the PIN entry
 *    prompt from the left of the screen, encoded as a string containing a
 *    base-10 integer.
 *
 *    The fourth frame contains the distance of the right side of the PIN entry
 *    prompt from the left of the screen, encoded as a string containing a
 *    base-10 integer.
 *
 *    The fifth frame contains the distance of the top of the PIN entry
 *    prompt from the top of the screen, encoded as a string containing a
 *    base-10 integer.
 *
 *    The sixth frame contains the number of seconds before PIN timeout
 *    occurs, or 0 for no timeout. It is encoded as a string containing a
 *    base-10 integer.
 *
 *    The seventh frame contains the minimum number of characters for a
 *    complete PIN, encoded as a string containing a base-10 integer.
 *
 *    The seventh frame contains the maximum number of characters for a
 *    complete PIN, encoded as a string containing a base-10 integer.
 *
 *
 * 6. Online processing result: After the "online" event has been received,
 *    send this message to deliver the result of the online processing to
 *    this service. Send the following frames:
 *
 *        ["contactless-emv", "online-result", "[status]", "[auth-code]",
 *         "[auth-data]", "[issuer-script]"]
 *
 *    The third frame must contain "approved", "declined", "error",
 *    "issuer-referral-approved" or "issuer-referral-declined".
 *
 *    The fourth frame must contain the authorization code as received from
 *    the host. It is expected to be encoded as a string containing
 *    hexadecimal characters, or a blank string.
 *
 *    The fifth frame must contain the authorization data as received from
 *    the host. It is expected to be encoded as a string containing
 *    hexadecimal characters, or a blank string.
 *
 *    The sixth frame must contain the issuer script, if any. It is expected
 *    to be encoded as a string containing hexadecimal characters, or a blank
 *    string.
 *
 *
 * 7. Start listening for contactless transaction: When no transaction is
 *    in progress, send this message to cause the contactless service to begin
 *    listening for a contactless card. Send the following frames:
 *
 *        ["contactless-emv", "start", "[timeout]"]
 *
 *    The third frame contains the amount of time to wait, in milliseconds,
 *    encoded as a string containing a base-10 number.
 *
 **/

#include "config.h"
#include <inttypes.h>
#include <time.h>
#include "services.h"
#include "util/detokenize_template.h"
#include "util/files.h"
#include "util/string_helpers.h"
#include "util/tlv.h"

#define d_ONLINE_KEYSET         0xC001
#define d_ONLINE_KEYINDEX       0x001A

static zactor_t  *service          = NULL;
static zsock_t   *pub              = NULL;
static zsock_t   *sub              = NULL;
static zpoller_t *sub_or_interrupt = NULL;

#if HAVE_CTOS && HAVE_EMVCL

#include <ctosapi.h>
#include <emv_cl.h>

// missing from header on MARS1000
ULONG EMVCL_CompleteEx(BYTE bAction, BYTE *baARC, UINT uiIADLen, BYTE *baIAD, UINT uiScriptLen, BYTE *baScript, EMVCL_RC_DATA_EX *stRCDataEx);

#define DELAY_MS             100
#define LUNA_CANCELLED      1024
#define WRONG_MSG_SIZE      1025
#define INVALID_TXN_TYPE    1026
#define INVALID_FORCE_VALUE 1027
#define UNKNOWN_CARD_TYPE   1028
#define PIN_ENTRY_FAILED    1029
#define NUM_TXN_ATTEMPTS 3

#define TXN_TYPE_SALE            0x00
#define TXN_TYPE_REFUND          0x20
#define TXN_TYPE_BALANCE_INQUIRY 0x30
#define TXN_TYPE_VERIFICATION    0x00

static struct {
  bool was_aborted;
  char *ksn;
  char *pin_block;
  char *cvm_result;
} current_txn;


/*
 * Waits for either an abort message, or a message with the specified topic.
 * If an abort is received or the service is terminated/interrupted, NULL is
 * returned. Otherwise the remainder of the message containing the specified
 * topic is returned. The message must be freed.
 */
zmsg_t *wait_for_message_or_abort(const char *topic) {
  if (current_txn.was_aborted)
    return NULL;

  while (1) {
    void *in = zpoller_wait(sub_or_interrupt, -1);
    if (!in) {
      LWARN("emv: contactless: interrupted, will abort");
      current_txn.was_aborted = true;
      return NULL;
    }

    if (in == sub) {
      zmsg_t *msg = zmsg_recv(in);
      char *frame = zmsg_popstr(msg);
      assert(!strcmp(frame, "contactless-emv"));
      free(frame);
      frame = zmsg_popstr(msg);
      if (!strcmp(frame, topic)) {
        LTRACE("emv: contactless: received message topic: %s", topic);
        free(frame);
        return msg;
      } else if (!strcmp(frame, "abort")) {
        LTRACE("emv: contactless: aborting");
        free(frame);
        zmsg_destroy(&msg);
        current_txn.was_aborted = true;
        return NULL;
      } else {
        // ignore events emitted by myself
        if (strcmp(frame, "error")                && strcmp(frame, "txn-started")             &&
            strcmp(frame, "message")              && strcmp(frame, "pin-verified-offline")    &&
            strcmp(frame, "issuer-script-result") && strcmp(frame, "txn-complete")            &&
            strcmp(frame, "ready-for-txn-data")   && strcmp(frame, "ready-for-app-selection") &&
            strcmp(frame, "single-app-selected")  && strcmp(frame, "performing-pin-entry")    &&
            strcmp(frame, "online")) {
          LWARN("emv: contactless: message '%s' is not expected at this time and will be ignored", frame);
        }
        free(frame);
      }
      zmsg_destroy(&msg);
    } else {
      LWARN("emv: contactless: received shutdown signal, will abort");
      current_txn.was_aborted = true;
      return NULL;
    }
  }
}

/*
 * Waits either a continue or an abort, and returns 0 or 1 respectively.
 */
int wait_for_continue_or_abort(void) {
  zmsg_t *msg = wait_for_message_or_abort("continue");
  if (msg) {
    zmsg_destroy(&msg);
    return 0;
  }
  return 1;
}

const char *emv_strerror(int res) {
  switch(res) {
    case d_EMVCL_NO_ERROR:                  return "Success";
    case d_EMVCL_RC_DATA:                   return "Transaction data received";
    case d_EMVCL_RC_SCHEME_SUPPORTED:       return "Payment scheme is supported";
    case d_EMVCL_PENDING:                   return "Pending";
    case d_EMVCL_TX_CANCEL:                 return "Transaction cancelled";
    case d_EMVCL_INIT_TAGSETTING_ERROR:     return "Tag init error";
    case d_EMVCL_INIT_CAPK_ERROR:           return "CAPK init error";
    case d_EMVCL_INIT_PARAMETER_ERROR:      return "Parameter init error";
    case d_EMVCL_INIT_CAPABILITY:           return "Capability init error";
    case d_EMVCL_INIT_REVOCATION_ERROR:     return "Revocation data init error";
    case d_EMVCL_INIT_EXCEPTION_FILE_ERROR: return "Exception file init error";
    case d_EMVCL_DATA_NOT_FOUND:            return "Data not found";
    case d_EMVCL_TRY_AGAIN:                 return "Try again";
    case d_EMVCL_RC_FAILURE:                return "General failure";
    case d_EMVCL_RC_INVALID_DATA:           return "Invalid data";
    case d_EMVCL_RC_INVALID_PARAM:          return "No such parameters";
    case d_EMVCL_RC_INVALID_SCHEME:         return "Invalid scheme";
    case d_EMVCL_RC_MORE_CARDS:             return "More than 1 card";
    case d_EMVCL_RC_NO_CARD:                return "Contactless card is not present";
    case d_EMVCL_RC_FALLBACK:               return "Fall back";
    case d_EMVCL_RC_DEK_SIGNAL:             return "DEK signal";
    default:                                return "Unknown";
  }
}

void log_emv_error(int res) {
  const char *msg = emv_strerror(res);
  LWARN("emv: contactless: error %u: %s", res, msg);
  zsock_send(pub, "ssis", "contactless-emv", "error", res, msg);
  (void) wait_for_continue_or_abort();
}

void publish_message(const char *type, const char *msg) {
  zsock_send(pub, "ssss", "contactless-emv", "message", type, msg);
  wait_for_continue_or_abort();
}

int publish_fatal_error(int code, const char *msg) {
  zsock_send(pub, "ssis", "contactless-emv", "error", code, msg);
  return LUNA_CANCELLED;
}

int recv_txn_data(int *total_amount, int *cashback_amount, int *txn_type, int *currency_code, int *force_online) {
  zsock_send(pub, "ss", "contactless-emv", "ready-for-txn-data");
  zmsg_t *msg = wait_for_message_or_abort("txn-data");
  if (zmsg_size(msg) != 3) {
    publish_fatal_error(WRONG_MSG_SIZE, "wrong message size");
    zmsg_destroy(&msg);
    return 1;
  }

  char *type_str = zmsg_popstr(msg);
  if (!strcmp(type_str, "sale"))                 *txn_type = TXN_TYPE_SALE;
  else if (!strcmp(type_str, "balance-inquiry")) *txn_type = TXN_TYPE_BALANCE_INQUIRY;
  else if (!strcmp(type_str, "refund"))          *txn_type = TXN_TYPE_REFUND;
  else if (!strcmp(type_str, "return"))          *txn_type = TXN_TYPE_REFUND;
  else if (!strcmp(type_str, "verification"))    *txn_type = TXN_TYPE_VERIFICATION;
  else {
    LERROR("emv: contactless: invalid txn type: %s", type_str);
    publish_fatal_error(INVALID_TXN_TYPE, "invalid txn type");
    free(type_str);
    zmsg_destroy(&msg);
    return 1;
  }
  free(type_str);
  
  char *force_str = zmsg_popstr(msg);
  if (!strcmp(force_str, "true")) {
    *force_online = d_TRUE;
  } else if (!strcmp(force_str, "false")) {
    *force_online = d_FALSE;
  } else {
    LERROR("emv: contactless: invalid force value: %s", force_str);
    publish_fatal_error(INVALID_FORCE_VALUE, "invalid force value (must be \"true\" or \"false\")");
    free(force_str);
    zmsg_destroy(&msg);
    return 1;
  }
  free(force_str);

  // TODO support currency code
  *currency_code = 0x0840;

  // TODO support cashback amount
  *cashback_amount = 0;

  char *total_amount_str = zmsg_popstr(msg);
  *total_amount = atoi(total_amount_str);
  LTRACE("emv: contactless: total amount: %d", *total_amount);
  free(total_amount_str);
  zmsg_destroy(&msg);
  return 0;
}

int begin_contactless(int total_amount, int cashback_amount,
                      int type, int currency_code, int force_online) {
  char emv_tags[1024];
  memset(emv_tags, 0, sizeof(emv_tags));
  sprintf(emv_tags + strlen(emv_tags), "9F02%02X%012d", 6, (unsigned) total_amount);
  sprintf(emv_tags + strlen(emv_tags), "9F03%02X%012d", 6, (unsigned) cashback_amount);
  sprintf(emv_tags + strlen(emv_tags), "9C%02X%02X",    1, (unsigned) type);
  sprintf(emv_tags + strlen(emv_tags), "5F2A%02X%04X",  2, (unsigned) currency_code);
  if (force_online) sprintf(emv_tags + strlen(emv_tags), "DF9F010101");
  LDEBUG("emv: contactless: transaction data: %s", emv_tags);
  size_t blob_len = strlen(emv_tags);
  char *blob = hex2bytes(emv_tags, &blob_len);
  return EMVCL_InitTransactionEx(force_online ? 5 : 4, (BYTE *) blob, blob_len);
}

void receive_online_result(void) {
  // will be populated with stuff we already know about the txn
  EMVCL_RC_DATA_EX result;

  zmsg_t *msg = wait_for_message_or_abort("online-result");
  if (zmsg_size(msg) != 4) {
    publish_fatal_error(WRONG_MSG_SIZE, "wrong message size");
    zmsg_destroy(&msg);
    EMVCL_CompleteEx(0x03, (BYTE *) "Z3", 0, (BYTE *) "", 0, (BYTE *) "", &result);
    return;
  }

  char *status = zmsg_popstr(msg);
  BYTE action = 0x03; // error
  if (!strcmp(status, "approved")) action = 0x01;
  else if (!strcmp(status, "declined")) action = 0x02;
  else if (!strcmp(status, "error")) action = 0x03;
  else {
    LERROR("emv: contactless: unexpected status %s, treating it as an error", status);
  }
  LTRACE("emv: contactless: result: status %s => %x", status, action);
  free(status);

  char *response_code_hex    = zmsg_popstr(msg);
  size_t response_code_len = strlen(response_code_hex);
  char *response_code = hex2bytes(response_code_hex, &response_code_len);
  LTRACE("emv: contactless: result: using response code %s", response_code_hex);
  if (!response_code) {
    LWARN("emv: contactless: response code '%s' is not a hex string, will indicate error instead", response_code_hex);
    // Z3 meaning unable to auth online, according to
    // https://www.visa.com/chip/merchants/grow-your-business/payment-technologies/credit-card-chip/docs/quick-chip-emv-specification.pdf
    response_code = strdup("Z3");
    response_code_len = 2;
  }
  if (response_code_len != 2) {
    LWARN("emv: contactless: response code '%s' is not the right length, will indicate error instead", response_code_hex);
    free(response_code);
    response_code = strdup("Z3");
    response_code_len = 2;
  }
  free(response_code_hex);

  char *issuer_auth_data_hex = zmsg_popstr(msg);
  size_t issuer_auth_data_len = strlen(issuer_auth_data_hex);
  char *issuer_auth_data = hex2bytes(issuer_auth_data_hex, &issuer_auth_data_len);
  LTRACE("emv: contactless: result: issuer auth data %s", issuer_auth_data_hex);
  if (!issuer_auth_data) {
    LWARN("emv: contactless: issuer auth data '%s' is not a hex string", issuer_auth_data);
    issuer_auth_data = strdup("");
    issuer_auth_data_len = 0;
  } else {
    if (issuer_auth_data_len < 8) {
      LWARN("emv: contactless: issuer auth data is too short (%d bytes)", issuer_auth_data_len);
    } else if (issuer_auth_data_len > 16) {
      LWARN("emv: contactless: issuer auth data is too long (%d bytes)", issuer_auth_data_len);
    }
  }
  free(issuer_auth_data_hex);

  char *issuer_script_hex    = zmsg_popstr(msg);
  LTRACE("emv: contactless: result: issuer script %s", issuer_script_hex);
  size_t issuer_script_len = strlen(issuer_script_hex);
  char *issuer_script = hex2bytes(issuer_script_hex, &issuer_script_len);
  if (!issuer_script) {
    LWARN("emv: contactless: couldn't parse issuer script data");
    issuer_script = strdup("");
  }
  free(issuer_script_hex);

  EMVCL_CompleteEx(action, (BYTE *) response_code,
                   issuer_auth_data_len, (BYTE *) issuer_auth_data,
                   issuer_script_len, (BYTE *) issuer_script, &result);
  free(response_code);
  free(issuer_auth_data);
  free(issuer_script);
  zmsg_destroy(&msg);
}

int perform_pin_entry(const char *pan) {
  zsock_send(pub, "ssis", "contactless-emv", "performing-pin-entry", 1, "online");
  zmsg_t *msg = wait_for_message_or_abort("pin-entry-ui");
  assert(msg != NULL);
  if (zmsg_size(msg) != 6) {
    publish_fatal_error(WRONG_MSG_SIZE, "wrong message size");
    zmsg_destroy(&msg);
    return 1;
  }

  char *tmp = NULL;
  int left, right, top, timeout, min_size, max_size;
  assert(tmp = zmsg_popstr(msg));
  left = (unsigned short) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  right = (unsigned short) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  top = (unsigned short) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  timeout = (unsigned long) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  min_size = (BYTE) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  max_size = (BYTE) atoi(tmp);
  free(tmp);

  LTRACE("emv: contactless: PIN entry UI: left = %d, right = %d, top = %d, "
                                          "min_size = %d, max_size = %d, timeout = %d",
                                          left, right, top, min_size, max_size, timeout);
  LINSEC("emv: contactless: PIN entry: PAN: %s", pan);

  // so now we need to send a message over to the pin entry service to begin
  // pin entry, and listen for all of its broadcasts. We need to perform UI
  // operations based on those broadcasts to emulate the behavior we get with
  // the built-in PIN pad during contact EMV. We also need to listen for abort
  // messages or interrupts from our main contactless socket.
  //
  // In this state machine, if err is 0 then we are not finished; if it is -1
  // then we have completed with no error; if it is 1 then we are to abort
  // returning an error.
  int err = 0;
  const char char_mask = '*';
  zsock_t *pinsock = zsock_new_sub(">" EVENTS_SUB_ENDPOINT, "pin-entry");
  zpoller_add(sub_or_interrupt, pinsock);
  LTRACE("emv: contactless: sending PIN entry start request");
  assert(pub != NULL);
  zsock_send(pub, "sssiiis", "pin-entry", "start", pan, min_size, max_size, timeout, "online");
  // draw background and get asterisk measurement
  LTRACE("emv: contactless: PIN entry: setting up UI");
  CTOS_LCDSelectMode(d_LCD_TEXT_MODE);
  LTRACE("emv: contactless: PIN entry: waiting for updates");
  int current_size = 0;
  while (!err) {
    void *in = zpoller_wait(sub_or_interrupt, -1);
    if (!in) {
      LWARN("emv: contactless: interrupted while performing PIN entry!");
      err = 1;
    } else if (in == pinsock) {
      char *tmp;
      zmsg_t *msg = zmsg_recv(pinsock);
      assert(msg && zmsg_size(msg) >= 2);
      
      // expecting "pin-entry" prefix
      tmp = zmsg_popstr(msg);
      assert(!strcmp(tmp, "pin-entry"));
      free(tmp);

      tmp = zmsg_popstr(msg);
      if (!strcmp(tmp, "error")) {
        // an error occurred, but we need to check if it was fatal
        char *errnum = zmsg_popstr(msg);
        char *errtext = zmsg_popstr(msg);
        char *fatal = zmsg_popstr(msg);
        LERROR("emv: contactless: error during PIN entry: %s (%s)", errtext, errnum);
        if (!strcmp(fatal, "true")) {
          err = 1;
        }
        free(errnum);
        free(errtext);
        free(fatal);
      } else if (!strcmp(tmp, "cancelled")) {
        LDEBUG("emv: contactless: PIN entry cancelled");
        current_txn.was_aborted = true;
        current_size = 0;
        err = 1;
      } else if (!strcmp(tmp, "complete")) {
        char *pin_length = zmsg_popstr(msg);
        char *pin_block = zmsg_popstr(msg);
        char *ksn = zmsg_popstr(msg);
        LDEBUG("emv: contactless: PIN entry complete (%s characters)", pin_length);
        current_size = atoi(pin_length);
        current_txn.ksn = ksn;
        current_txn.pin_block = pin_block;
        free(pin_length);
        err = -1;
      } else if (!strcmp(tmp, "started")) {
        LTRACE("emv: contactless: PIN entry has been started");
      } else if (!strcmp(tmp, "key-added") || !strcmp(tmp, "key-removed")) {
        char *was_added_or_removed = zmsg_popstr(msg);
        char *new_size_str = zmsg_popstr(msg);
        int new_size = atoi(new_size_str);
        LTRACE("emv: contactless: %s (new length: %d characters)", tmp, new_size);
        free(was_added_or_removed);
        free(new_size_str);
        int incr = (new_size > current_size ? 1 : -1);
        int i;
        for (i = current_size; i != new_size; i += incr) {
          // if we're removing a character, draw white space. Else, draw
          // an asterisk.
          int x = left + i;
          if (incr > 0) {
            LTRACE("emv: contactless: drawing mask at %d", x);
            CTOS_LCDTPutchXY(x, top, (UCHAR) char_mask);
          } else {
            LTRACE("emv: contactless: clearing mask at %d", x);
            CTOS_LCDTGotoXY(x - 1, top);
            CTOS_LCDTClear2EOL();
          }
        }
        current_size = new_size;
      } else {
        LWARN("emv: contactless: unhandled PIN entry message: %s", tmp);
      }

      free(tmp);
      zmsg_destroy(&msg);
    } else if (in == sub) {
      LWARN("emv: contactless: message was not expected at this time and will be ignored:");
      zmsg_t *msg = zmsg_recv(sub);
      zmsg_dump(msg);
      zmsg_destroy(&msg);
    } else {
      // in must == the service pipe, but we conspicuously try not to consume
      // the message itself, instead we abort the txn so that the message
      // can be consumed later at the top level so that the service can
      // terminate properly.
      LWARN("emv: contactless: received shutdown signal during PIN entry");
      err = 1;
      current_txn.was_aborted = true;
    }
  }

  CTOS_LCDSelectMode(d_LCD_GRAPHIC_MODE);
  zpoller_remove(sub_or_interrupt, pinsock);
  zsock_destroy(&pinsock);
  if (err == -1) { return 0; }
  return err;
}

void process_txn(int txn_type, EMVCL_RC_DATA_EX *txn) {
  EMVCL_RC_DATA_ANALYZE txn_analysis;
  memset(&txn_analysis, 0, sizeof(txn_analysis));

  // Process txn data into stuff we're willing to give to lua
  char scheme[256];
  char datetime[16];
  switch(txn->bSID) {
    case d_EMVCL_SID_VISA_OLD_US:              sprintf(scheme, "visa-old-us");        break;
    case d_EMVCL_SID_VISA_WAVE_2:              sprintf(scheme, "visa-wave-2");        break;
    case d_EMVCL_SID_VISA_WAVE_QVSDC:          sprintf(scheme, "visa-qvsdc");         break;
    case d_EMVCL_SID_VISA_WAVE_MSD:            sprintf(scheme, "visa-msd");           break;
    case d_EMVCL_SID_PAYPASS_MAG_STRIPE:       sprintf(scheme, "paypass-magstripe");  break;
    case d_EMVCL_SID_PAYPASS_MCHIP:            sprintf(scheme, "paypass-mchip");      break;
    case d_EMVCL_SID_JCB_WAVE_2:               sprintf(scheme, "jcb-wave-2");         break;
    case d_EMVCL_SID_JCB_WAVE_QVSDC:           sprintf(scheme, "jcb-qvsdc");          break;
    case d_EMVCL_SID_AE_EMV:                   sprintf(scheme, "amex-emv");           break;
    case d_EMVCL_SID_AE_MAG_STRIPE:            sprintf(scheme, "amex-magstripe");     break;
    case d_EMVCL_SID_DISCOVER:                 sprintf(scheme, "discover-zip");       break;
    case d_EMVCL_SID_JCB_EMV:                  sprintf(scheme, "jcb-emv");            break;
    case d_EMVCL_SID_JCB_MSD:                  sprintf(scheme, "jcb-msd");            break;
    case d_EMVCL_SID_JCB_LEGACY:               sprintf(scheme, "jcb-legacy");         break;
    case d_EMVCL_SID_DISCOVER_DPAS:            sprintf(scheme, "discover-dpas");      break;
    case d_EMVCL_SID_INTERAC_FLASH:            sprintf(scheme, "interac-flash");      break;
    case d_EMVCL_SID_MEPS_MCCS:                sprintf(scheme, "meps-mccs");          break;
    case d_EMVCL_SID_CUP_QPBOC:                sprintf(scheme, "cup-qpboc");          break;
    case d_EMVCL_SID_DISCOVER_DPAS_MAG_STRIPE: sprintf(scheme, "discover-magstripe"); break;
    default:                                   sprintf(scheme, "unknown (%x)", txn->bSID);
  }
  sprintf(datetime, "%*s", 14, (const char *) txn->baDateTime);
  char *track1_hex = bytes2hex((char *) txn->baTrack1Data, txn->bTrack1Len);
  char *track2_hex = bytes2hex((char *) txn->baTrack2Data, txn->bTrack2Len);
  token_id track1   = create_token(track1_hex, strlen(track1_hex), "EMV Contactless Track 1");
  token_id track2   = parse_emv_track2_equiv(track2_hex);
  LTRACE("emv: contactless: response packet: success: scheme=%s, datetime=%s, track1=%d, track2=%d",
         scheme, datetime, (signed) track1, (signed) track2);
  free(track1_hex);
  free(track2_hex);

  // Analyze the txn result and then coerce that too
  EMVCL_AnalyzeTransactionEx(txn, &txn_analysis);
  const char *status = NULL;
  switch(txn_analysis.usTransResult) {
    /* refund is always approved online, and we actually expect EMV kernel to
       generate a decline for a successful refund. *shrug* */
    case 0x02:
      status = "approved";
      break;
    case 0x03:
      if (txn_type == TXN_TYPE_REFUND) status = "online";
      else status = "declined";
      break;
    case 0x04:
      status = "online";
      break;
    default:
      LERROR("emv: contactless: unexpected txn status: %u", txn_analysis.usTransResult);
  }
  assert(status != NULL);
  LTRACE("emv: contactless: txn status: %s", status);

  const char *cvm_mode = NULL;
  current_txn.cvm_result = bytes2hex((char *) txn_analysis.baCVMResults, sizeof(txn_analysis.baCVMResults));
  switch(txn_analysis.bCVMAnalysis) {
    case 0x00: cvm_mode = "none"; break;
    case 0x01: cvm_mode = "signature"; break;
    case 0x02: cvm_mode = "online PIN"; break;
    case 0x03: cvm_mode = "failure"; break;
    case 0x04: cvm_mode = "not available"; break;
    case 0x05: cvm_mode = "confirmation code verified"; break;
    default: LERROR("emv: contactless: unexpected CVM mode: %u", txn_analysis.bCVMAnalysis);
  }
  assert(cvm_mode != NULL);
  LTRACE("emv: contactless: CVM mode: %s", cvm_mode);
  LTRACE("emv: contactless: CVM result: %s", current_txn.cvm_result);

  // have to parse the tlv data in order to find the pan and seq number
  tlv_t *head = NULL, *tmp = NULL;
  if (tlv_parse(txn->baChipData, txn->usChipDataLen, &head)) {
    LWARN("emv: contactless: couldn't completely process chip data");
  }
  if (tlv_parse(txn->baAdditionalData, txn->usAdditionalDataLen, &head)) {
    LWARN("emv: contactless: couldn't completely process additional data");
  }

  if (!strcmp(cvm_mode, "online PIN")) {
    HASH_FIND_STR(head, "\x5a", tmp); // PAN
    char *pan_token_str;
    if (tmp) {
      char *pan_str = bytes2hex((char *) tmp->value, tmp->value_length);

      // strip out padding characters, if any
      int l = 0, i;
      for(i = 0; i < strlen(pan_str); ++i) {
        if(!isdigit(pan_str[i])) {
          break;
        }
        l++;
      }
      if(l > 31) l = 31;
      *(pan_str + l) = '\0';

      char *pan_str_mask = strdup(pan_str);
      for (i = 1; i < strlen(pan_str_mask) - 4; i++)
        *(pan_str_mask + i) = '*';
      token_id pan_token = create_token(pan_str, strlen(pan_str), pan_str_mask);
      pan_token_str = calloc(64, sizeof(char));
      sprintf(pan_token_str, "%s%u%s", TOKEN_PREFIX, (unsigned) pan_token, TOKEN_SUFFIX);
      free(pan_str);
      free(pan_str_mask);
    } else {
      LWARN("emv: contactless: no PAN?");
      pan_token_str = strdup("");
    }

    if (perform_pin_entry(pan_token_str)) {
      publish_fatal_error(PIN_ENTRY_FAILED, "PIN entry failed");
      tlv_freeall(&head);
      return;
    }
    free(pan_token_str);
  } else {
    current_txn.ksn = strdup("");
    current_txn.pin_block = strdup("");
  }

  const char *card_type = NULL;
  switch(txn->bSID) {
    case d_EMVCL_SID_VISA_OLD_US:              card_type = "MSD"; break;
    case d_EMVCL_SID_VISA_WAVE_2:              card_type = "MSD"; break;
    case d_EMVCL_SID_VISA_WAVE_QVSDC:          card_type = "EMV"; break;
    case d_EMVCL_SID_VISA_WAVE_MSD:            card_type = "MSD"; break;
    case d_EMVCL_SID_PAYPASS_MAG_STRIPE:       card_type = "MSD"; break;
    case d_EMVCL_SID_PAYPASS_MCHIP:            card_type = "EMV"; break;
    case d_EMVCL_SID_JCB_WAVE_2:               card_type = "MSD"; break;
    case d_EMVCL_SID_JCB_WAVE_QVSDC:           card_type = "EMV"; break;
    case d_EMVCL_SID_AE_EMV:                   card_type = "EMV"; break;
    case d_EMVCL_SID_AE_MAG_STRIPE:            card_type = "MSD"; break;
    case d_EMVCL_SID_JCB_EMV:                  card_type = "EMV"; break;
    case d_EMVCL_SID_JCB_MSD:                  card_type = "MSD"; break;
    case d_EMVCL_SID_JCB_LEGACY:               card_type = "MSD"; break;
    case d_EMVCL_SID_DISCOVER_DPAS:            card_type = "EMV"; break;
    case d_EMVCL_SID_INTERAC_FLASH:            card_type = "MSD"; break;
    case d_EMVCL_SID_MEPS_MCCS:                card_type = "MSD"; break;
    case d_EMVCL_SID_CUP_QPBOC:                card_type = "MSD"; break;
    case d_EMVCL_SID_DISCOVER_DPAS_MAG_STRIPE: card_type = "MSD"; break;
    case d_EMVCL_SID_DISCOVER:
      // Find AID because when scheme == discover-zip only the AID can tell us
      // if it's contactless EMV vs MSD.
      tmp = NULL;
      HASH_FIND_STR(head, "\x9F\x06", tmp); // Application ID
      if (tmp) {
        char *aid = bytes2hex((char *) tmp->value, tmp->value_length);
        LDEBUG("emv: contactless: transaction AID: %s", aid);
        if (!strcmp(aid, "A0000003241010")) card_type = "MSD";
        else card_type = "EMV";
        free(aid);
      }
      else {
        LWARN("emv: contactless: transaction AID not found");
        card_type = "EMV";
      }
      tmp = NULL;
      break;
    default:
      publish_fatal_error(UNKNOWN_CARD_TYPE, "could not determine card type");
      tlv_freeall(&head);
      return;
  }

  assert(card_type != NULL);
  LDEBUG("emv: contactless: card type: %s", card_type);

  int is_visa_offline_spending_amount_available = (int) txn_analysis.bVisaAOSAPresent;
  char visa_offline_spending_amount[sizeof(txn_analysis.baVisaAOSA) + 1];
  sprintf(visa_offline_spending_amount, "%.*s", sizeof(txn_analysis.baVisaAOSA), txn_analysis.baVisaAOSA);
  int offline_data_authentication_failed = (int) txn_analysis.bODAFail;
  int pan_seq_num = -1;
  tmp = NULL;
  HASH_FIND_STR(head, "\x5F\x34", tmp); // PAN seq number
  if (tmp) pan_seq_num = atoi((char *) tmp->value);
  LTRACE("emv: contactless: pan sequence number: %d", pan_seq_num);
  LTRACE("emv: contactless: visa aosa available: %d", is_visa_offline_spending_amount_available);
  LTRACE("emv: contactless: visa aosa: %s", visa_offline_spending_amount);
  LTRACE("emv: contactless: offline data authentication failed: %d", offline_data_authentication_failed);

  char *receipt_data = NULL;
  tlv_sanitize(&head); // remove any tags which may contain sensitive data
  size_t receipt_data_len = tlv_encode(&head, &receipt_data);
  char *receipt_data_hex = bytes2hex(receipt_data, receipt_data_len);
  free(receipt_data);

  char track1str[64], track2str[64];
  sprintf(track1str, "%s%d%s", TOKEN_PREFIX, (int) track1,   TOKEN_SUFFIX);
  sprintf(track2str, "%s%d%s", TOKEN_PREFIX, (int) track2,   TOKEN_SUFFIX);

  if (!strcmp(status, "online")) {
    zsock_send(pub, "sssssissss", "contactless-emv", "online",
                    current_txn.pin_block, current_txn.ksn,
                    receipt_data_hex, pan_seq_num, track2str, "true",
                    card_type, scheme);
    receive_online_result();
  }

  zsock_send(pub, "sssssisss", "contactless-emv", "txn-complete", status,
                  !strcmp(cvm_mode, "signature") ? "true" : "false",
                  receipt_data_hex, pan_seq_num, track2str,
                  !strcmp(cvm_mode, "online") ? "true" : "false", "true",
                  card_type);

  free(receipt_data_hex);
  tlv_freeall(&head);
}

void start_txn(int timeout, int attempts) {
  int err;
  LTRACE("emv: contactless: starting txn with timeout %d, %d attempts remaining", timeout, attempts);
  zsock_send(pub, "ssi", "contactless-emv", "txn-started", attempts);
  if (wait_for_continue_or_abort()) return;

  // gather txn data
  int total_amount, cashback_amount, txn_type, currency_code, force_online;
  if (recv_txn_data(&total_amount, &cashback_amount, &txn_type, &currency_code, &force_online)) {
    LERROR("emv: contactless: failed to receive transaction data, will abort");
    return;
  }

  // set timeout length
  short timeout_s = (short) timeout;
  if (timeout_s != timeout) {
    LWARN("emv: contactless: timeout %d exceeds supported range; will be treated as %d", timeout, timeout_s);
  } else {
    LDEBUG("emv: contactless: setting timeout to %d", timeout_s);
  }
  EMVCL_PARAMETER_DATA parm;
  parm.uiNoP = 1;
  parm.uiIndex[0] = 0x0002;
  parm.uiLen[0]   = 2;
  parm.baData[0][0] = timeout_s >> 8;
  parm.baData[0][1] = timeout_s % 0xFF;
  err = EMVCL_SetParameter(&parm);
  switch(err) {
    case d_OK:                     break;
    case d_EMVCL_RC_INVALID_PARAM: LWARN("emv: contactless: error while setting timeout to %d: invalid parameter", timeout_s); break;
    case d_EMVCL_RC_FAILURE:       LWARN("emv: contactless: error while setting timeout to %d: internal failure", timeout_s); break;
    default:                       LWARN("emv: contactless: unknown error while setting timeout to %d", timeout_s);
  }

  // Start the transaction; this is non-blocking so we'll do polling next.
  err = begin_contactless(total_amount, cashback_amount, txn_type, currency_code, force_online);
  switch(err) {
    case d_EMVCL_NO_ERROR:
    case d_EMVCL_RC_DATA:
      break; // no error
    default:
      EMVCL_CancelTransaction();
      log_emv_error(err);
      if (current_txn.was_aborted)
        return;
  }

  EMVCL_RC_DATA_EX txn_out;
  memset(&txn_out,      0, sizeof(txn_out));
  while (err == d_EMVCL_NO_ERROR) {
    // check if any abort message is waiting on socket
    if (zpoller_wait(sub_or_interrupt, 0) == sub) {
      if (wait_for_continue_or_abort()) {
        LINFO("emv: contactless: cancelling transaction");
        EMVCL_CancelTransaction();
        current_txn.was_aborted = true;
        return;
      }
    }

    err = EMVCL_PollTransactionEx(&txn_out, 100); // timeout in ms?
    // If err indicates we should poll again, then it's not an err.
    // Otherwise, allow the loop to end, and handle the error outside of
    // the loop.
    switch(err) {
      case d_EMVCL_NO_ERROR:
      case d_EMVCL_PENDING:
        err = d_EMVCL_NO_ERROR;
    }
  }
  // process any error from the loop above
  if (err != d_EMVCL_RC_DATA &&
    /* ok, err could be d_EMVCL_RC_FAILURE for a refund/return transaction
       and this is apparently expected behavior, so DON'T treat that specific
       case as an error. */
      !(err == d_EMVCL_RC_FAILURE && txn_type == TXN_TYPE_REFUND)
     ) {
    EMVCL_CancelTransaction();
    publish_fatal_error(err, emv_strerror(err));
    return;
  }

  process_txn(txn_type, &txn_out);
}

void emvcl_message(int code, const char **type, const char **msg) {
  switch(code) {
    case d_EMVCL_USR_REQ_MSG_CARD_READ_OK:
    case d_EMVCL_USR_REQ_MSG_APPROVED:
    case d_EMVCL_USR_REQ_MSG_APPROVED_SIGN:
    case d_EMVCL_USR_REQ_MSG_DECLINED:
    case d_EMVCL_USR_REQ_MSG_INSERT_CARD:
    case d_EMVCL_USR_REQ_MSG_SEE_PHONE:
    case d_EMVCL_USR_REQ_MSG_AUTHORISING_PLEASE_WAIT:
    case d_EMVCL_USR_REQ_MSG_CLEAR_DISPLAY:
    case d_EMVCL_USR_REQ_MSG_ENTER_PIN:
    case d_EMVCL_USR_REQ_MSG_REMOVE_CARD:
    case d_EMVCL_USR_REQ_MSG_WELCOME:
    case d_EMVCL_USR_REQ_MSG_PRESENT_CARD:
    case d_EMVCL_USR_REQ_MSG_PROCESSING:
    case d_EMVCL_USR_REQ_MSG_INSERT_OR_SWIPE_CARD:
      *type = "info";
      break;
    default:
      *type = "error";
  }

  switch(code) {
    case d_EMVCL_USR_REQ_MSG_CARD_READ_OK:               *msg = "Card read OK"; break;
    case d_EMVCL_USR_REQ_MSG_TRY_AGAIN:                  *msg = "Please try again"; break;
    case d_EMVCL_USR_REQ_MSG_APPROVED:                   *msg = "Approved"; break;
    case d_EMVCL_USR_REQ_MSG_APPROVED_SIGN:              *msg = "Approved, please sign"; break;
    case d_EMVCL_USR_REQ_MSG_DECLINED:                   *msg = "Declined"; break;
    case d_EMVCL_USR_REQ_MSG_ERROR_OTHER_CARD:           *msg = "Error, please use other card"; break;
    case d_EMVCL_USR_REQ_MSG_INSERT_CARD:                *msg = "Please insert card"; break;
    case d_EMVCL_USR_REQ_MSG_SEE_PHONE:                  *msg = "See phone"; break;
    case d_EMVCL_USR_REQ_MSG_AUTHORISING_PLEASE_WAIT:    *msg = "Authorizing, please wait"; break;
    case d_EMVCL_USR_REQ_MSG_CLEAR_DISPLAY:              *msg = "Clear display"; break;
    case d_EMVCL_USR_REQ_MSG_ENTER_PIN:                  *msg = "Enter PIN"; break;
    case d_EMVCL_USR_REQ_MSG_PROCESSING_ERR:             *msg = "Processing error"; break;
    case d_EMVCL_USR_REQ_MSG_REMOVE_CARD:                *msg = "Please remove card"; break;
    case d_EMVCL_USR_REQ_MSG_WELCOME:                    *msg = "Welcome"; break;
    case d_EMVCL_USR_REQ_MSG_PRESENT_CARD:               *msg = "Please present card"; break;
    case d_EMVCL_USR_REQ_MSG_PROCESSING:                 *msg = "Processing, please wait"; break;
    case d_EMVCL_USR_REQ_MSG_INSERT_OR_SWIPE_CARD:       *msg = "Please insert or swipe card"; break;
    case d_EMVCL_USR_REQ_MSG_PRESENT_1_CARD_ONLY:        *msg = "Please present only 1 card"; break;
    case d_EMVCL_USR_REQ_MSG_NO_CARD:                    *msg = "No card"; break;
    case d_EMVCL_USR_REQ_MSG_NA:                         *msg = "Status N/A"; break;
    default: *msg = "Status unknown";
  }
}

void emvcl_on_show_message(BYTE which_kernel, EMVCL_USER_INTERFACE_REQ_DATA *data) {
  const char *type = NULL, *msg = NULL;
  emvcl_message(data->bMessageIdentifier, &type, &msg);
  LDEBUG("emv: contactless: show message: %u: %u: %s: %s", which_kernel, data->bMessageIdentifier, type, msg);
  LDEBUG("emv: contactless:   status: %u", data->bStatus);
  LDEBUG("emv: contactless:   hold time: %*s", 3, (const char *) data->baHoldTime);
  LDEBUG("emv: contactless:   language preference: %*s", 8, (const char *) data->baLanguagePreference);
  LDEBUG("emv: contactless:   value qualifier: %u", data->bValueQualifier);
  LDEBUG("emv: contactless:   value: %*s", 6, (const char *) data->baValue);
  LDEBUG("emv: contactless:   currency code: %*s", 2, (const char *) data->baCurrencyCode);
  zsock_send(pub, "ssss", "contactless-emv", "message", type, msg);
  wait_for_continue_or_abort();
}

void emvcl_service(zsock_t *pipe, void *arg) {
  (void) arg;
  memset(&current_txn, 0, sizeof(current_txn));
  pub = zsock_new_pub(">" EVENTS_PUB_ENDPOINT);
  sub = zsock_new_sub(">" EVENTS_SUB_ENDPOINT, "contactless-emv");
  sub_or_interrupt = zpoller_new(pipe, sub, NULL);

  assert(pub);
  assert(sub);
  
  int have_contactless = 1;
  EMVCL_INIT_DATA emvcl_init;
  char *filename = find_readable_file(NULL, "emvcl_config.xml");
  if (!filename) {
    LERROR("emv: contactless: could not find readable file: emvcl_config.xml");
    goto cleanup;
  }
  memset(&emvcl_init, 0, sizeof(emvcl_init));
  emvcl_init.bConfigFilenameLen = strlen(filename);
  emvcl_init.pConfigFilename = (BYTE *) filename;
  emvcl_init.stOnEvent.OnShowMessage = emvcl_on_show_message;
  // EMVCL_SetDebug(TRUE, d_EMVCL_DEBUG_PORT_USB);
  ULONG res2 = EMVCL_Initialize(&emvcl_init);
  if (res2 != d_EMVCL_NO_ERROR) {
    LERROR("emv: contactless: initialization failed: %lu", res2);
    LERROR("emv: contactless: will be unavailable");
    have_contactless = 0;
  }
  LINFO("emv: contactless: service initialized");
  zsock_signal(pipe, 0);

  while (have_contactless) {
    void *in = zpoller_wait(sub_or_interrupt, DELAY_MS);
    if (!in) {
      if (!zpoller_expired(sub_or_interrupt)) {
        LWARN("emv: contactless: interrupted!");
        break;
      }
    } else if (in == pipe) {
      LINFO("emv: contactless: shutting down");
      break;
    } else if (in == sub) {
      zmsg_t *msg = wait_for_message_or_abort("start");
      // even if we got 'abort' just now ... we are not in an active txn state
      // until we receive a 'start'
      current_txn.was_aborted = false;
      if (msg != NULL) {
        int timeout = 15000;
        char *tmp = zmsg_popstr(msg);
        if (tmp) { timeout = atoi(tmp); free(tmp); }
        start_txn(timeout, NUM_TXN_ATTEMPTS);

        // after txn, free any txn resources
        if (current_txn.ksn)        free(current_txn.ksn);
        if (current_txn.pin_block)  free(current_txn.pin_block);
        if (current_txn.cvm_result) free(current_txn.cvm_result);
        memset(&current_txn, 0, sizeof(current_txn));
        zmsg_destroy(&msg);
      }
    }
  }


cleanup:
  zpoller_destroy(&sub_or_interrupt);
  zsock_destroy(&sub);
  zsock_destroy(&pub);
  sub_or_interrupt = NULL;
  sub = NULL;
  pub = NULL;
  if (filename) free(filename);
  service = NULL;
  LDEBUG("emv: contactless: shutdown complete");
}

#else  // HAVE_CTOS

void emvcl_service(zsock_t *pipe, void *arg) {
  LWARN("emv: contactless: not supported on this device");
  zsock_signal(pipe, 1);
}

#endif // HAVE_CTOS

int init_emv_contactless_service(void) {
  if (!service) service = zactor_new(emvcl_service, NULL);
  (void) pub;
  (void) sub;
  (void) sub_or_interrupt;
  return 0;
}

bool is_emv_contactless_service_running(void) { return !!service; }

void shutdown_emv_contactless_service(void) {
  if (service) zactor_destroy(&service);
  service = NULL;
}
