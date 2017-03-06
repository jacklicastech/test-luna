/**
 * Performs contact EMV transactions. No UI operations are performed. Receive
 * messages from this service by connecting to the "inproc://events/sub"
 * endpoint. Send messages to this service by sending them to the
 * "inproc://events/pub" endpoint.
 * 
 *
 * This service is sensitive to the following settings:
 *
 * * emv.contact.active_configuration_index
 *
 *   When this setting is modified, the service will shut down and then
 *   reinitialize using the setting's new value if possible. If the setting
 *   value is out of range or if it hasn't been set, a warning will be logged
 *   and it will be set to 0.
 *
 *
 * The following types of socket message may originate on the service side:
 *
 * 1. EMV card inserted: Published when the user inserts a contact EMV card.
 *    Sends the following frames:
 *
 *        ["contact-emv", "card-inserted"]
 *
 *
 * 2. EMV card removed: Published when the user removes a contact EMV card.
 *    Sends the following frames:
 *
 *        ["contact-emv", "card-removed"]
 *
 *
 * 3. Error: Published when an EMV processing error occurs. Sends the
 *    following frames:
 *
 *        ["contact-emv", "error", "[errno]", "[message]"]
 *
 *    The third frame contains a number identifying the error, encoded as
 *    a string containing a base-10 number, after the backspace has been
 *    processed.
 *
 *    The fourth frame contains a human-readable text message explaining the
 *    error.
 *
 *
 * 4. Starting transaction: Published when transaction processing has begun.
 *    Sends the following frames:
 *
 *        ["contact-emv", "txn-started", "[attempts-remaining]"]
 *
 *    The third frame contains the number of attempts remaining, encoded as a
 *    string containing a base-10 number. This number includes the current
 *    attempt, so it will never be less than 1.
 *
 *
 * 5. Message: Published when the EMV kernel decides a message should be shown
 *    to the end user. Sends the following frames:
 *
 *        ["contact-emv", "message", "[type]", "[text]"]
 *
 *    The third frame contains either "error" or "info", indicating the type
 *    of message to be displayed.
 *
 *    The fourth frame contains the human-readable text of the message to
 *    display. Note that the language can change depending on the EMV card
 *    being used.
 *
 *
 * 6. PIN verified offline: Published when the EMV kernel performs PIN
 *    verification offline. Not sent if PIN verification will occur online.
 *    Sends the following frames:
 *
 *        ["contact-emv", "pin-verified-offline", "[result]"]
 *
 *    The third frame contains one of the values "ok", "failed", "blocked" or
 *    "failed+blocked".
 *
 *
 * 7. Issuer script result: Published when an issuer script has finished
 *    processing and a result is ready. Sends the following frames:
 *
 *        ["contact-emv", "issuer-script-result", "[hex]"]
 *
 *    The third frame is encoded as a string containing a hexadecimal value,
 *    which was received from the EMV kernel.
 *
 *
 * 8. Transaction complete: Published at the end of a transaction. No further
 *    events for this transaction will be published, though an EMV card
 *    removed (#2) message may still be published. Sends the following frames:
 *
 *        ["contact-emv", "txn-complete", "[result]", "[signature-required]",
 *         "[emv-tags]", "[pan-seq-number]", "[track2]", "[online]",
 *         "[contactless]", "EMV"]
 *
 *    The third frame contains one of the values "approved", "declined",
 *    "error" or "unknown". Note that the value "unknown" indicates a bug in
 *    the contact EMV service, and shouldn't be an expected result in
 *    practice. The value "error" occurs when a processing has failed after
 *    numerous attempts. Therefore, fallback should be attempted if the
 *    "error" value occurs.
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
 *    was received and no "run transaction online" (#13) event was received,
 *    this transaction can safely be assumed to be an offline transaction.
 *
 *    The ninth frame contains "false" to signal that this was not a
 *    contactless transaction.
 *
 *    The tenth frame is always "EMV" to indicate that this is an EMV card.
 *
 *
 * 9. Ready for transaction data: Published when the EMV kernel is prepared to
 *    receive transaction data such as amount, transaction type, etc. Sends
 *    the following frames:
 *
 *        ["contact-emv", "ready-for-txn-data"]
 *
 * 
 * 10. Ready for app selection: Published when the EMV kernel has detected
 *     multiple applications on the card and must be instructed as to which
 *     app should be used. Sends the following frames:
 *
 *        ["contact-emv", "ready-for-app-selection", "[app-name]", ...]
 *
 *     The third and subsequent frames contain human-readable application
 *     names, which can be presented to an end user for selection. When
 *     sending the response to this message, the third frame (which is the
 *     first application) should be treated as index 0.
 *
 *
 * 11. Single app has been selected: Published when the EMV kernel has
 *     detected only one viable application on the card, and would like to
 *     automatically select it. A confirmation may need to be shown to the end
 *     user to allow them a chance to abort the transaction. Sends the
 *     following frames:
 *
 *         ["contact-emv", "single-app-selected", "[app-name]",
 *          "[must-confirm]"]
 *
 *     The third frame contains a human-readable application name, which can
 *     be shown to the end user.
 *
 *     The fourth frame contains "true" if the EMV kernel has indicated that
 *     end-user confirmation is required in order to proceed; "false"
 *     otherwise.
 *
 * 
 * 12. Performing PIN entry: Published when PIN entry is required. You must
 *     send a "pin-entry-ui" message describing the UI for PIN entry. Sends
 *     the following frames:
 *
 *         ["contact-emv", "performing-pin-entry", "[attempts_remaining]",
 *          "[type]"]
 *
 *     The third frame contains the number of PIN entry attempts remaining.
 *
 *     The fourth frame contains the value "online" or "offline", representing
 *     whether the PIN will be verified online or offline.
 *
 *
 * 13. Run transaction online: Published when the transaction must be
 *     completed online. Online processing must be performed and the result
 *     must be sent to this service before the transaction can be completed.
 *     Sends the following frames:
 *
 *         ["contact-emv", "online", "[pin-block]", "[ksn]", "[emv-data]",
 *          "[pan-seq-number]", "[track2]", "[contactless]", "EMV", ""]
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
 *     The eigth frame contains "false" to signal that this was not a
 *     contactless transaction.
 *
 *     The ninth frame is always "EMV" to indicate that this is an EMV card.
 *
 *     The tenth frame is reserved for future use.
 *
 *
 * The following types of socket message may originate on the client side:
 *
 * 1. Continue: Sent to indicate that the transaction may proceed. The
 *    following message types will stop transaction processing while waiting
 *    for either this message or an Abort (#2) message:
 *
 *        card-inserted
 *        error
 *        message
 *        txn-started
 *        pin-verified-offline
 *        issuer-script-result
 *        single-app-selected
 * 
 *    Send the following frames:
 *
 *        ["contact-emv", "continue"]
 *
 *
 * 2. Abort: Sent to indicate that the transaction should be aborted at the
 *    next opportunity. This message can be sent at any time and will result
 *    in an "error" event representing the fact that the transaction was
 *    aborted. Send the following frames:
 *
 *        ["contact-emv", "abort"]
 *
 * 
 * 3. Select app: When multiple applications are available, and a
 *    "ready-for-app-selection" event has been published, the service will
 *    block until either this message or an Abort (#2) is published. Use this
 *    message to choose one of the available applications. Send the following
 *    frames:
 *
 *        ["contact-emv", "select-app-index", "[index]"]
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
 *        ["contact-emv", "txn-data", "[type]", "[force-online]",
 *         ["txn-amount"]]
 *
 *    The third frame must contain "sale", "balance-inquiry", "refund",
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
 *        ["contact-emv", "online-result", "[status]", "[auth-code]",
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
 **/

#include "config.h"
#include <inttypes.h>
#include <time.h>
#include "plugin.h"
#include "util/detokenize_template.h"
#include "util/files.h"
#include "util/string_helpers.h"

#define d_ONLINE_KEYSET         0xC001
#define d_ONLINE_KEYINDEX       0x001A

static zactor_t  *service          = NULL;
static zsock_t   *pub              = NULL;
static zsock_t   *sub              = NULL;
static zpoller_t *sub_or_interrupt = NULL;

#if HAVE_CTOS && HAVE_LIBCAEMVL2

#include <ctosapi.h>
#include <emvaplib.h>

#define DELAY_MS 100
#define LUNA_CANCELLED 1024
#define WRONG_MSG_SIZE 1025

static struct {
  bool was_aborted;
  bool is_pin_online;
  bool is_online;
  char *ksn;
  char *pin_block;
  char *emv_data;
  char *pan;
} current_txn;


void start_keypad_service(void) {
  plugin_t *plugin = find_plugin_by_name("keypad");
  if (plugin && plugin->service.init) {
    int err = plugin->service.init(0, NULL);
    if (err) {
      LERROR("emv: contact: couldn't start keypad service: it failed with error %d", err);
    }
  } else {
    LWARN("emv: contact: can't start keypad service");
  }
}

bool stop_keypad_service(void) {
  plugin_t *plugin = find_plugin_by_name("keypad");
  if (plugin && plugin->service.is_running) {
    if (plugin->service.is_running()) {
      if (plugin->service.shutdown) {
        LTRACE("emv: contact: stopping keypad service");
        plugin->service.shutdown();
        return true;
      } else {
        LWARN("emv: contact: can't stop keypad service");
      }
    } else {
      LTRACE("emv: contact: keypad service is not running");
    }
  } else {
    LWARN("emv: contact: can't query whether keypad service is running; won't try to stop it");
  }

  return false;
}


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
      LWARN("emv: contact: interrupted, will abort");
      current_txn.was_aborted = true;
      return NULL;
    }

    if (in == sub) {
      zmsg_t *msg = zmsg_recv(in);
      char *frame = zmsg_popstr(msg);
      assert(!strcmp(frame, "contact-emv"));
      free(frame);
      frame = zmsg_popstr(msg);
      if (!strcmp(frame, topic)) {
        LTRACE("emv: contact: received message topic: %s", topic);
        free(frame);
        return msg;
      } else if (!strcmp(frame, "abort")) {
        LTRACE("emv: contact: aborting");
        free(frame);
        zmsg_destroy(&msg);
        current_txn.was_aborted = true;
        return NULL;
      } else {
        // ignore events emitted by myself
        if (strcmp(frame, "card-inserted")        && strcmp(frame, "card-removed")            &&
            strcmp(frame, "error")                && strcmp(frame, "txn-started")             &&
            strcmp(frame, "message")              && strcmp(frame, "pin-verified-offline")    &&
            strcmp(frame, "issuer-script-result") && strcmp(frame, "txn-complete")            &&
            strcmp(frame, "ready-for-txn-data")   && strcmp(frame, "ready-for-app-selection") &&
            strcmp(frame, "single-app-selected")  && strcmp(frame, "perform-pin-entry")       &&
            strcmp(frame, "online")) {
          LWARN("emv: contact: message '%s' is not expected at this time and will be ignored", frame);
        }
        free(frame);
      }
      zmsg_destroy(&msg);
    } else {
      LWARN("emv: contact: received shutdown signal, will abort");
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

void log_emv_error(USHORT res) {
  const char *msg = NULL;
  #define _case(a, b) case a: msg = b; break;
  switch(res) {
    _case(d_EMVAPLIB_OK,                                        "success (BUG?)");
    _case(d_EMVAPLIB_ERR_SET_DEFAULT_DATA_ERROR,                "could not set terminal data to terminal buffer");
    _case(d_EMVAPLIB_ERR_TERM_DATA_MISSING,                     "terminal data is missing");
    _case(d_EMVAPLIB_ERR_ONLY_1_AP_NO_FALLBACK,                 "only one application and it is blocked");
    _case(d_EMVAPLIB_ERR_FUNCTION_NOT_SUPPORTED,                "function not supported");
    _case(d_EMVAPLIB_ERR_CRITICAL_ERROR,                        "critical error");
    _case(d_EMVAPLIB_ERR_DATA_BUFFER_EXCEEDED,                  "buffer size exceeded");
    _case(d_EMVAPLIB_ERR_NO_AP_FOUND,                           "application not found");
    _case(d_EMVAPLIB_ERR_EVENT_CONFIRMED,                       "onAppSelectedConfirm callback is not set");
    _case(d_EMVAPLIB_ERR_EVENT_SELECTED,                        "onAppList callback is not set");
    _case(d_EMVAPLIB_ERR_EVENT_GET_TXNDATA,                     "onTxnDataGet callback is not set");
    _case(d_EMVAPLIB_ERR_EVENT_VERSION,                         "invalid callback index version");
    _case(d_EMVAPLIB_ERR_GAC1_6985_FALLBACK,                    "GenAC1 returned status 0x6985");
    _case(d_EMVAPLIB_ERR_FORCE_ACCEPTANCE,                      "force acceptance");
    _case(d_EMVAPLIB_ERR_EVENT_ONLINE,                          "onTxnOnline callback is not set");
    _case(d_EMVAPLIB_ERR_EVENT_BUFFER_DEFINE,                   "pAuthorizationCode, pIssuerAuthenticationData and/or pIssuerScript are not set");
    _case(d_EMVAPLIB_ERR_EVENT_TXN_RESULT,                      "onTxnResult callback is not set");
    _case(d_EMVAPLIB_ERR_SEND_APDU_CMD_FAIL,                    "failed to send APDU command");
    _case(d_EMVAPLIB_ERR_ERROR_9F4A_RULE,                       "tag 9F4A rule error");
    _case(d_EMVAPLIB_ERR_KEY_NO_FOUND,                          "key not found");
    _case(d_EMVAPLIB_ERR_PAN_NOT_SAME,                          "PAN is not the same");
    _case(d_EMVAPLIB_ERR_DDOL_MISS,                             "the DDOL is missing");
    _case(d_EMVAPLIB_ERR_INTERNAL_AUTHENTICATE_FAIL,            "internal authentication failure");
    _case(d_EMVAPLIB_ERR_NO_OFFLINE_DATA_AUTH_MATCH,            "no matching offline authentication method");
    _case(d_EMVAPLIB_ERR_MISS_APP_EXPIRATION_DATE,              "the application effective date is missing");
    _case(d_EMVAPLIB_ERR_CARDHOLDER_VER_NOT_SUPP,               "cardholder verification is not supported");
    _case(d_EMVAPLIB_ERR_ERR_CVM_LIST_MISSING,                  "CVM list is missing");
    _case(d_EMVAPLIB_ERR_GET_DATA_CMD_ERROR,                    "error issuing the get data command");
    _case(d_EMVAPLIB_ERR_DIS_TLV_TAG_ZERO,                      "TLV tag second byte is zero");
    _case(d_EMVAPLIB_ERR_DIS_TLV_FAIL,                          "TLV error while dismantling");
    _case(d_EMVAPLIB_ERR_DIS_TLV_EXCEED_MAX_LEN,                "tag value length exceeds the maximum length");
    _case(d_EMVAPLIB_ERR_DDOL_NOT_HAVE_9F37,                    "DDOL is missing tag 9F37");
    _case(d_EMVAPLIB_ERR_SDA_DATA_ERROR,                        "static data authentication error");
    _case(d_EMVAPLIB_ERR_SDA_ALGORITHM_NOT_SUPPORT,             "static data authentication algorithm is not supported");
    _case(d_EMVAPLIB_ERR_DDA_DATA_ERROR,                        "dynamic data authentication error");
    _case(d_EMVAPLIB_ERR_DDA_ALGORITHM_NOT_SUPPORT,             "dynamic data authentication algorithm is not supported");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_NOT_EXIST,                 "issuer public key certificate is missing");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_FORMAT_ERROR,              "issuer public key certificate format is invalid");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_IIN_PAN_NOT_SAME,          "issuer ID number and PAN are not the same");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_REVOCATION_FOUND,          "issuer public key certificate has been revoked");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_ALGORITHM_NOT_SUPPORT,     "issuer public key certificate algorithm is not supported");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_LENGTH_ERROR,              "issuer public key certificate is an invalid length");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_EXPIRATION_DATE,           "issuer public key certificate has expired");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_HASH_NOT_MATCH,            "issuer public key certificate hash mismatch");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_EXPONENT_NOT_EXIST,        "issuer public key certificate exponent does not exist");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_REMAINDER_MISSING,         "issuer public key pomander is missing");
    _case(d_EMVAPLIB_ERR_READ_DATA_TAG_NOT_70,                  "read tag data error: not in 70 format");
    _case(d_EMVAPLIB_ERR_ICC_CERT_NOT_EXIST,                    "ICC public key certificate does not exist");
    _case(d_EMVAPLIB_ERR_ICC_CERT_FORMAT_ERROR,                 "ICC public key certificate format error");
    _case(d_EMVAPLIB_ERR_ICC_CERT_ALGORITHM_NOT_SUPPORT,        "ICC public key certificate algorithm not supported");
    _case(d_EMVAPLIB_ERR_ICC_CERT_LENGTH_ERROR,                 "ICC public key certificate is an invalid length");
    _case(d_EMVAPLIB_ERR_ICC_CERT_HASH_NOT_MATCH,               "ICC public key certificate hash mismatch");
    _case(d_EMVAPLIB_ERR_ICC_CERT_EXPIRATION_DATE,              "ICC public key certificate has expired");
    _case(d_EMVAPLIB_ERR_ICC_CERT_EXPONENT_NOT_EXIST,           "ICC public key certificate exponent does not exist");
    _case(d_EMVAPLIB_ERR_ICC_ISSUER_PK_NOT_EXIST,               "ICC public key certificate issuer PK does not exist");
    _case(d_EMVAPLIB_ERR_ICC_CERT_REMAINDER_MISSING,            "ICC public key certificate pomander is missing");
    _case(d_EMVAPLIB_ERR_CVM_PLAIN_TEXT_PIN_NOT_KEYIN,          "Plaintext PIN verification performed by ICC but cardholder didn't enter PIN");
    _case(d_EMVAPLIB_ERR_CVM_PLAIN_TEXT_PIN_TRY_LIMIT_EXCEEDED, "Plaintext PIN verification attempt limit exceeded");
    _case(d_EMVAPLIB_ERR_CVM_PLAIN_TEXT_PIN_OK,                 "Plaintext PIN verified successfully");
    _case(d_EMVAPLIB_ERR_CVM_PLAIN_TEXT_PIN_WRONG,              "Plaintext PIN verification failed");
    _case(d_EMVAPLIB_ERR_CVM_PLAIN_TEXT_UNKNOW_SW12,            "Plaintext PIN failed because the APDU response data was unrecognized");
    _case(d_EMVAPLIB_ERR_CVM_TERMINAL_NOT_SUPPORT_SPECIFY_CVM,  "terminal CVM support is unspecified");
    _case(d_EMVAPLIB_ERR_CVM_ENC_PIN_ONLINE_PIN_NOT_KEYIN,      "encrypted PIN online verification was performed by ICC but cardholder didn't enter PIN");
    _case(d_EMVAPLIB_ERR_CVM_ENCIPHERED_PIN_NOT_KEYIN,          "encrypted PIN verification was performed by ICC but cardholder didn't enter PIN");
    _case(d_EMVAPLIB_ERR_CVM_ENCIPHERED_PIN_TRY_LIMIT_EXCEEDED, "encrypted PIN verification limit exceeded");
    _case(d_EMVAPLIB_ERR_CVM_ENCIPHERED_PIN_UNKNOW_SW12,        "encrypted PIN failed because the APDU response data was unrecognized");
    _case(d_EMVAPLIB_ERR_CVM_ENCIPHERED_PIN_GET_RN_UNKNOW_SW12, "the APDU response data was unrecognized");
    _case(d_EMVAPLIB_ERR_MISSING_TERMINAL_DATA,                 "terminal data is missing");
    _case(d_EMVAPLIB_ERR_CARD_DATA_MULTIPLE,                    "multiple card data");
    _case(d_EMVAPLIB_ERR_ISSUER_CERT_CAPKI_NOT_EXIST,           "issuer public key certificate CAPKI does not exist");
    _case(d_EMVAPLIB_ERR_CVM_PLAIN_TEXT_PIN_BLOCK,              "ICC has been blocked");
    _case(LUNA_CANCELLED,                                       "transaction cancelled");
    case 0x1404:
      // card could not be read, perform fallback.
      zsock_send(pub, "ssss", "contact-emv", "message", "error", "Please swipe card");
      (void) wait_for_continue_or_abort();
      msg = "perform fallback";
      break;
    default: msg = "unknown";
  }
  LWARN("emv: contact: error %u: %s", res, msg);
  zsock_send(pub, "ssis", "contact-emv", "error", (int) res, msg);
  (void) wait_for_continue_or_abort();
}

static int emv_pan_seq_num(void) {
  BYTE pan_seq_num[512];
  USHORT len = 512;
  memset(pan_seq_num, 0, sizeof(pan_seq_num));
  if (EMV_DataGet(EMV_TAG_APP_PRIMARY_ACCT_NUMBER_SEQ_NUMBER, &len, pan_seq_num) == d_EMVAPLIB_OK) {
    char *buf = bytes2hex((char *) pan_seq_num, strlen((char *) pan_seq_num));
    int result = atoi(buf);
    free(buf);
    return result;
  } else {
    LWARN("emv: contact: couldn't query PAN sequence number");
    return -1;
  }
}

#define ACCUM_TLV(tag, buf, len, out, hex)                                   \
                            hex = bytes2hex((char *) buf, len);              \
                            out = realloc(out, strlen(out) + 64 + len * 2);  \
                            LDEBUG("emv: contact: tag " #tag " (%04X) length: %d",    \
                                   tag, len);                                \
                            if (len <= 127) {                                \
                              sprintf((char *) (out + strlen((char *) out)), \
                                      "%X%02X%s", tag, len, hex);            \
                            } else if (len <= 255) {                         \
                              sprintf((char *) (out + strlen((char *) out)), \
                                      "%X%02X%02X%s", tag, 128 | 1,          \
                                      len, hex);                             \
                            } else {                                         \
                              sprintf((char *) (out + strlen((char *) out)), \
                                      "%X%02X%02X%02X%s", tag, 128 | 2,      \
                                      len / 256, len % 256, hex);            \
                            }                                                \
                            free(hex);

#define ACCUM_DE55(tag) len = 512;                                           \
                        res = EMV_DataGet(tag, &len, buf);                   \
                        if (res == d_EMVAPLIB_OK) {                          \
                          char *hex;                                         \
                          ACCUM_TLV(tag, buf, len, out, hex);                \
                        } else {                                             \
                          LWARN("emv: contact: could not query data for tag " #tag    \
                                " (%04X): %u", tag, res);                    \
                          len = 0;                                           \
                        }

// HACK because I don't know how to parse tag names like 0xDFC8, the last byte
// indicates another byte should follow but it doesn't. So I'm swapping out
// with parseable, equivalent-meaning tag names.
#define ACCUM_DE55_SWAP(tag, alternative) len = 512;                         \
                        res = EMV_DataGet(tag, &len, buf);                   \
                        if (res == d_EMVAPLIB_OK) {                          \
                          char *hex;                                         \
                          ACCUM_TLV(alternative, buf, len, out, hex);        \
                        } else {                                             \
                          LWARN("emv: contact: could not query data for tag " #tag    \
                                " (%04X): %u", tag, res);                    \
                          len = 0;                                           \
                        }

token_id emv_construct_track2_equiv(void) {
  BYTE buf[512];
  USHORT len = 512;
  USHORT res = EMV_DataGet(d_TAG_TRACK2_EQUIVALENT_DATA, &len, buf);
  if (res == d_EMVAPLIB_OK) {
    char *x = bytes2hex((char *) buf, len);
    token_id track2_token = parse_emv_track2_equiv(x);
    free(x);
    return track2_token;
  } else {
    LWARN("emv: contact: could not fetch track2 equivalent");
    return 0;
  }
}

char *emv_construct_de55(void) {
  char *out = strdup("");
  BYTE buf[512];
  USHORT len;
  USHORT res;

  ACCUM_DE55(EMV_TAG_APP_LABEL);
  ACCUM_DE55(EMV_TAG_APP_PREFERRED_NAME);
  ACCUM_DE55(EMV_TAG_APP_USAGE_CONTROL);
  ACCUM_DE55(EMV_TAG_ISSUER_ACTION_CODE_DEFAULT);
  ACCUM_DE55(EMV_TAG_ISSUER_ACTION_CODE_DENIAL);
  ACCUM_DE55(EMV_TAG_ISSUER_ACTION_CODE_ONLINE);
  ACCUM_DE55(EMV_TAG_APP_ID_TERMINAL);
  ACCUM_DE55(EMV_TAG_TRANSACTION_STATUS_INFORMATION);
  ACCUM_DE55(EMV_TAG_AUTHORIZATION_RESPONSE_CODE);
  ACCUM_DE55(EMV_TAG_APP_CRYPTOGRAM);
  ACCUM_DE55(EMV_TAG_APP_EXPIRATION_DATE);
  ACCUM_DE55(EMV_TAG_APP_INTERCHANGE_PROFILE);
  ACCUM_DE55(EMV_TAG_APP_PRIMARY_ACCT_NUMBER_SEQ_NUMBER);
  ACCUM_DE55(EMV_TAG_APP_TRANSACTION_COUNTER);
  ACCUM_DE55(EMV_TAG_CRYPTOGRAM_INFORMATION_DATA);
  ACCUM_DE55(EMV_TAG_DEDICATED_FILE_NAME);
  ACCUM_DE55(EMV_TAG_ISSUER_APPLICATION_DATA);
  ACCUM_DE55(EMV_TAG_TRANSACTION_CATEGORY_CODE);
  ACCUM_DE55(EMV_TAG_AUTHORIZED_AMT_ASCII);
  ACCUM_DE55(EMV_TAG_CASHBACK_AMT_ASCII);
  ACCUM_DE55(EMV_TAG_APP_VERSION_NUMBER);
  ACCUM_DE55(EMV_TAG_CARDHOLDER_VERIFICATION_METHOD_RESULTS);
  ACCUM_DE55(EMV_TAG_INTERFACE_DEVICE_SERIAL_NUMBER);
  ACCUM_DE55(EMV_TAG_POS_ENTRY_MODE);
  ACCUM_DE55(EMV_TAG_TERMINAL_CAPABILITIES);
  ACCUM_DE55(EMV_TAG_TERMINAL_COUNTRY_CODE);
  ACCUM_DE55(EMV_TAG_TERMINAL_VERIFICATION_RESULTS);
  ACCUM_DE55(EMV_TAG_TRANSACTION_CURRENCY_CODE);
  ACCUM_DE55(EMV_TAG_TRANSACTION_DATE);
  ACCUM_DE55(EMV_TAG_TRANSACTION_TYPE);
  ACCUM_DE55(EMV_TAG_UNPREDICTABLE_NUMBER);
  ACCUM_DE55(EMV_TAG_ISSUER_AUTHENTICATION_DATA);
  ACCUM_DE55(EMV_TAG_TERMINAL_APP_VERSION_NUMBER);
  ACCUM_DE55(EMV_TAG_TERMINAL_TYPE);
  ACCUM_DE55_SWAP(EMV_TAG_TERMINAL_ACTION_CODE_DEFAULT, 0xDF8120);
  ACCUM_DE55_SWAP(EMV_TAG_TERMINAL_ACTION_CODE_DENIAL,  0xDF8121);
  ACCUM_DE55_SWAP(EMV_TAG_TERMINAL_ACTION_CODE_ONLINE,  0xDF8122);

  // If txn sequence counter is not present, populate one ourselves with STAN,
  // which is just a 6-digit monotonically incrementing counter.
  ACCUM_DE55(EMV_TAG_TRANSACTION_SEQUENCE_COUNTER);
  if (res != d_EMVAPLIB_OK) {
    zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
    char *stan = NULL;
    if (!settings_get(settings, 1, "txn.stan", &stan)) {
      unsigned int stani = 0;
      if (stan == NULL || strlen(stan) == 0) {
        if (stan != NULL) free(stan);
      } else {
        stani = (unsigned) atoi(stan);
        free(stan);
      }
      stani = (stani % 999999) + 1;
      LDEBUG("emv: contact: using %d (0x%x) STAN for txn sequence number", stani, stani);
      sprintf((char *) (out + strlen((char *) out)), "%X%02X%06X",
              EMV_TAG_TRANSACTION_SEQUENCE_COUNTER, 3, stani);
      sprintf((char *) buf, "%u", stani);
      settings_set(settings, 1, "txn.stan", buf);
    } else {
      LWARN("emv: contact: could not query current STAN value");
    }
    zsock_destroy(&settings);
  }

  LINSEC("DE55 EMV data: %s", out);
  return out;
}

void emv_on_display_show(char *msg) {
  zsock_send(pub, "ssss", "contact-emv", "message", "info", msg);
  wait_for_continue_or_abort();
}

void emv_on_error(char *msg) {
  zsock_send(pub, "ssss", "contact-emv", "message", "error", msg);
  wait_for_continue_or_abort();
}

void emv_on_config_active(BYTE *active_index) {
  zsock_t *settings = zsock_new_req(SETTINGS_ENDPOINT);
  char *index_str = NULL;
  settings_get(settings, 1, "emv.contact.active_configuration_index", &index_str);
  if (index_str == NULL || !strcmp(index_str, "")) {
    LINFO("emv: contact: configuration index not specified, will not change default index %d", (int) *active_index);
  } else {
    int new_index = atoi(index_str);
    LINFO("emv: contact: setting configuration index to %d (was %d)", new_index, (int) *active_index);
    *active_index = (BYTE) new_index;
  }
  zsock_destroy(&settings);
}

int publish_fatal_error(int code, const char *msg) {
  zsock_send(pub, "ssis", "contact-emv", "error", code, msg);
  return LUNA_CANCELLED;
}

USHORT emv_on_txn_data_get(EMV_TXNDATA *out) {
  char *tmp;
  out->Version = 0x01;
  zsock_send(pub, "ss", "contact-emv", "ready-for-txn-data");
  zmsg_t *msg = wait_for_message_or_abort("txn-data");
  if (!msg) return LUNA_CANCELLED;
  if (zmsg_size(msg) != 3)
    return (USHORT) publish_fatal_error(WRONG_MSG_SIZE, "invalid message size");
  
  // txn_type
  tmp = zmsg_popstr(msg);
  LTRACE("emv: contact: txn type: %s", tmp);
  if (!strcmp(tmp, "sale"))                                  out->bTxnType = 0x00;
  else if (!strcmp(tmp, "balance-inquiry"))                  out->bTxnType = 0x30;
  else if (!strcmp(tmp, "refund") || !strcmp(tmp, "return")) out->bTxnType = 0x20;
  else if (!strcmp(tmp, "verification"))                     out->bTxnType = 0x00;
  else {
    LWARN("emv: contact: unrecognized transaction type: %s; should be one of: sale, balance-inquiry, refund, verification", tmp);
    free(tmp);
    zmsg_destroy(&msg);
    return d_EMVAPLIB_ERR_FUNCTION_NOT_SUPPORTED;
  }
  free(tmp);

  // is_force_online
  tmp = zmsg_popstr(msg);
  LTRACE("emv: contact: txn force online: %s", tmp);
  out->isForceOnline = !strcmp(tmp, "true");
  free(tmp);

  // amount
  tmp = zmsg_popstr(msg);
  LTRACE("emv: contact: txn amount: %s", tmp);
  out->ulAmount = (unsigned long) atol(tmp);
  free(tmp);

  // fields not exposed over zmq
  out->bPOSEntryMode = 0x05;
  CTOS_RTC rtc;
  if (CTOS_RTCGet(&rtc) == d_OK) {
    sprintf((char *) out->TxnDate, "%02u%02u%02u", rtc.bYear, rtc.bMonth, rtc.bDay);
    sprintf((char *) out->TxnTime, "%02u%02u%02u", rtc.bHour, rtc.bMinute,rtc.bSecond);
  } else {
    LWARN("emv: contact: unable to set transaction date/time");
    zmsg_destroy(&msg);
    return d_EMVAPLIB_ERR_CRITICAL_ERROR;
  }

  zmsg_destroy(&msg);
  return d_EMVAPLIB_OK;
}

USHORT emv_on_app_list(BYTE num_apps, char app_labels[][d_LABEL_STR_SIZE+1],
                              BYTE *selected_app_index) {
  int index, i;
  zmsg_t *msg = zmsg_new();
  for (i = num_apps - 1; i >= 0; i--) {
    zmsg_pushstr(msg, app_labels[i]);
  }
  zmsg_pushstr(msg, "ready-for-app-selection");
  zmsg_pushstr(msg, "contact-emv");
  zmsg_send(&msg, pub);
  
  msg = wait_for_message_or_abort("select-app-index");
  if (!msg) return LUNA_CANCELLED;
  if (zmsg_size(msg) != 1)
    return (USHORT) publish_fatal_error(WRONG_MSG_SIZE, "invalid message size");
  char *tmp = zmsg_popstr(msg);
  index = atoi(tmp);
  zmsg_destroy(&msg);

  if (index >= 0 && index < num_apps) {
    LTRACE("emv: contact: selecting app index %d", index);
    *selected_app_index = (BYTE) index;
  } else {
    LINFO("emv: contact: app selection was out of bounds, will abort");
    current_txn.was_aborted = true;
    return LUNA_CANCELLED;
  }

  return d_EMVAPLIB_OK;
}

USHORT emv_on_app_selected_confirm(BOOL required, BYTE *label, BYTE label_len) {
  char *label_str = calloc(1, (label_len + 1) * sizeof(char));
  snprintf(label_str, label_len, "%s", (char *) label);
  zsock_send(pub, "sssi", "contact-emv", "single-app-selected", label_str, (int) required);

  if (wait_for_continue_or_abort())
    return LUNA_CANCELLED;
  return d_EMVAPLIB_OK;
}

/*
 * Stores the next KSN value into `*out`. If an error occurs, `*out` is not
 * modified. Returns `OK` on success, otherwise a normalized error code
 * is returned.
 *
 * If successful, the value in `*out` must be freed.
 */
int get_ksn(char **out) {
  BYTE length = 10;
  BYTE ksnbuf[length];
  int res = CTOS_KMS2DUKPTGetKSN(d_ONLINE_KEYSET, d_ONLINE_KEYINDEX, ksnbuf, &length);
  if(res != d_OK) {
    LWARN("emv: contact: unable to get KSN for online PIN entry: %x", res);
    return res;
  } else {
    *out = bytes2hex((char *) ksnbuf, length);
    return d_OK;
  }
}

void emv_on_get_pin_notify(BYTE pin_type, USHORT remaining_count,
                           BOOL *use_internal_pin_entry,
                           DEFAULT_GETPIN_FUNC_PARA *params) {
  *use_internal_pin_entry = d_TRUE;
  memset(params, 0, sizeof(DEFAULT_GETPIN_FUNC_PARA));
  current_txn.is_pin_online = (pin_type == d_NOTIFY_ONLINE_PIN);

  char pan[512];
  USHORT pan_len = 512;
  memset(pan, 0, sizeof(pan));
  if (EMV_DataGet(EMV_TAG_APP_PRIMARY_ACCT_NUMBER, &pan_len, (BYTE *) pan) == d_EMVAPLIB_OK) {
    current_txn.pan = bytes2hex((const char *) pan, pan_len);
    pan_len = strlen(current_txn.pan);
    LINSEC("emv: contact: PAN is %s", current_txn.pan);

    // strip out padding characters, if any
    int l = 0, i;
    for(i = 0; i < pan_len; ++i) {
      if(!isdigit(current_txn.pan[i])) {
        break;
      }
      l++;
    }
    if(l > 31) l = 31;
    *(current_txn.pan + l) = '\0';
    pan_len = l;
    LINSEC("emv: contact: postprocessed PAN is %s", current_txn.pan);
  } else {
    pan_len = 0;
    LWARN("emv: contact: couldn't get PAN, so PIN entry may fail");
  }

  if (pin_type == d_NOTIFY_ONLINE_PIN) {
    get_ksn(&current_txn.ksn);
    params->ONLINEPIN_PARA.CipherKeyIndex = 0x001A;
    params->ONLINEPIN_PARA.CipherKeySet   = 0xC001;
    params->ONLINEPIN_PARA.bPANLen = pan_len;
    memcpy(params->ONLINEPIN_PARA.baPAN, current_txn.pan, pan_len);
  } else {
    current_txn.ksn = strdup("");
  }

  zsock_send(pub, "ssis", "contact-emv", "performing-pin-entry", remaining_count,
                  (pin_type == d_NOTIFY_ONLINE_PIN ? "online" : "offline"));
  zmsg_t *msg = wait_for_message_or_abort("pin-entry-ui");
  if (zmsg_size(msg) != 6) {
    LWARN("emv: contact: invalid message size, will abort the txn");
    current_txn.was_aborted = true;
    zmsg_destroy(&msg);
    return;
  }

  stop_keypad_service();

  char *tmp = NULL;
  assert(tmp = zmsg_popstr(msg));
  params->usLineLeft_X = (unsigned short) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  params->usLineRight_X = (unsigned short) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  params->usLinePosition_Y = (unsigned short) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  params->ulTimeout = (unsigned long) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  params->bPINDigitMinLength = (BYTE) atoi(tmp);
  free(tmp);

  assert(tmp = zmsg_popstr(msg));
  params->bPINDigitMaxLength = (BYTE) atoi(tmp);
  free(tmp);

  params->IsRightToLeft = 0;
  params->IsReverseLine = d_FALSE;
}

void emv_on_offline_pin_verify_result(USHORT result) {
  start_keypad_service();
  if (current_txn.was_aborted) return;

  const char *msg;
  switch(result) {
    case d_PIN_RESULT_OK:          msg = "ok"; break;
    case d_PIN_RESULT_FAIL:        msg = "failed"; break;
    case d_PIN_RESULT_BLOCKED:     msg = "blocked"; break;
    case d_PIN_RESULT_FAILBLOCKED: msg = "failed+blocked"; break;
    default:
      msg = "unknown";
      LWARN("emv: contact: unknown PIN verification result: %x", result);
  }
  zsock_send(pub, "sss", "contact-emv", "pin-verified-offline", msg);
  (void) wait_for_continue_or_abort();
}

void emv_on_txn_online(ONLINE_PIN_DATA *pin_data,
                       EMV_ONLINE_RESPONSE_DATA *response) {
  start_keypad_service();
  if (current_txn.was_aborted) {
    response->bAction = d_ONLINE_ACTION_UNABLE;
    return;
  }

  current_txn.is_online = true;
  current_txn.emv_data  = emv_construct_de55();
  int      pan_seq_num  = emv_pan_seq_num();
  token_id track2_equiv = emv_construct_track2_equiv();
  char track2_equiv_str[64];
  sprintf(track2_equiv_str, "%s%d%s", TOKEN_PREFIX, (int) track2_equiv, TOKEN_SUFFIX);
  current_txn.pin_block = bytes2hex((char *) pin_data->pPIN, pin_data->bPINLen);
  LTRACE("emv: contact: sending: online");

  assert(current_txn.pin_block != NULL);
  assert(current_txn.emv_data  != NULL);
  if (current_txn.ksn == NULL) current_txn.ksn = strdup("");
  
  LINSEC("emv: contact:   pin block: %s", current_txn.pin_block);
  LINSEC("emv: contact:   ksn: %s", current_txn.ksn);
  LINSEC("emv: contact:   emv: %s", current_txn.emv_data);
  LINSEC("emv: contact:   pan seq: %d", pan_seq_num);
  LINSEC("emv: contact:   track 2: %s", track2_equiv_str);
  zsock_send(pub, "sssssissss", "contact-emv", "online", current_txn.pin_block,
                  current_txn.ksn, current_txn.emv_data, pan_seq_num,
                  track2_equiv_str, "false", "EMV", "");
  LTRACE("emv: contact: waiting for online result");
  zmsg_t *msg = wait_for_message_or_abort("online-result");
  if (!msg) {
    response->bAction = d_ONLINE_ACTION_UNABLE;
    return;
  }

  char *tmp = zmsg_popstr(msg);
  LTRACE("emv: contact: online status: %s", tmp);
  if (!strcmp(tmp, "approved"))                      response->bAction = d_ONLINE_ACTION_APPROVAL;
  else if (!strcmp(tmp, "partial"))                  response->bAction = d_ONLINE_ACTION_APPROVAL;
  else if (!strcmp(tmp, "declined"))                 response->bAction = d_ONLINE_ACTION_DECLINE;
  else if (!strcmp(tmp, "error"))                    response->bAction = d_ONLINE_ACTION_UNABLE;
  else if (!strcmp(tmp, "issuer-referral-approved")) response->bAction = d_ONLINE_ACTION_ISSUER_REFERRAL_APPR;
  else if (!strcmp(tmp, "issuer-referral-declined")) response->bAction = d_ONLINE_ACTION_ISSUER_REFERRAL_DENY;
  else {
    LWARN("emv: contact: unexpected online transaction result: %s", tmp);
    LWARN("emv: contact: setting transaction result to 'error'");
    response->bAction = d_ONLINE_ACTION_UNABLE;
  }
  free(tmp);

  size_t len;
  tmp = zmsg_popstr(msg);
  len = strlen(tmp);
  if (len == 0) {
    if (response->bAction == d_ONLINE_ACTION_APPROVAL)
      response->pAuthorizationCode = (BYTE *) strdup("\x30\x30");
    else if (response->bAction == d_ONLINE_ACTION_DECLINE)
      response->pAuthorizationCode = (BYTE *) strdup("\x35\x35");
  } else {
    response->pAuthorizationCode = (BYTE *) hex2bytes(tmp, &len);
  }
  LTRACE("emv: contact: online auth code: %s", response->pAuthorizationCode);
  free(tmp);

  zframe_t *frame = zmsg_pop(msg);
  len = zframe_size(frame);
  LTRACE("emv: returned issuer auth data (%llu bytes): %.*s", (long long unsigned) len,
                                                              len, (const char *) zframe_data(frame));
  response->pIssuerAuthenticationData = (BYTE *) hex2bytes((char *) zframe_data(frame), &len);
  response->IssuerAuthenticationDataLen = len;
  dump_blob_as_hex(response->pIssuerAuthenticationData, len, "emv: issuer auth data as BCD: ");
  zframe_destroy(&frame);

  frame = zmsg_pop(msg);
  len = zframe_size(frame);
  LTRACE("emv: returned issuer script (%llu bytes): %.*s", (long long unsigned) len,
                                                           len, (const char *) zframe_data(frame));
  response->pIssuerScript = (BYTE *) hex2bytes((char *) zframe_data(frame), &len);
  response->IssuerScriptLen = len;
  dump_blob_as_hex(response->pIssuerScript, len, "emv: issuer script as BCD: ");
  zframe_destroy(&frame);
}

// void emv_on_total_amount_get(BYTE *pan, BYTE pan_len, ULONG *amount) {
//   assert(emv);
//   zsock_send(emv, "ss")
// }

void emv_on_txn_result(BYTE result, BOOL is_signature_required) {
  const char *result_str;
  switch(result) {
    case d_TXN_RESULT_APPROVAL:
      result_str = "approved";
      break;
    case d_TXN_RESULT_DECLINE:
      result_str = "declined";
      is_signature_required = (BOOL) 0;
      break;
    default:
      LWARN("emv: contact: unexpected transaction result: %u", result);
      result_str = "unknown";
  }

  int      pan_seq_num  = emv_pan_seq_num();
  token_id track2_equiv = emv_construct_track2_equiv();
  char track2_equiv_str[64];
  sprintf(track2_equiv_str, "%s%d%s", TOKEN_PREFIX, (int) track2_equiv, TOKEN_SUFFIX);

  // This can happen if the txn is rejected before EMV data is queried, i.e.
  // with ATM cards which are rejected out of hand.
  if (current_txn.emv_data == NULL) current_txn.emv_data = strdup("");

  LINSEC("emv: contact: result: string: %s", result_str);
  LINSEC("emv: contact: result: sig required: %d", is_signature_required);
  LINSEC("emv: contact: result: emv data: %s", current_txn.emv_data);
  LINSEC("emv: contact: result: pan seq num: %d", pan_seq_num);
  LINSEC("emv: contact: result: track 2: %s", track2_equiv_str);
  LINSEC("emv: contact: result: is online: %d", current_txn.is_online);

  zsock_send(pub, "sssssissss", "contact-emv", "txn-complete", result_str,
                  (is_signature_required ? "true" : "false"),
                  current_txn.emv_data, pan_seq_num, track2_equiv_str,
                  (current_txn.is_online ? "true" : "false"), "false", "");
}

void emv_on_txn_issuer_script_result(BYTE *pScriptResult, USHORT pScriptResultLen) {
  LTRACE("emv: contact: have issuer script result (%u bytes)", pScriptResultLen);
  char *buf = bytes2hex((char *) pScriptResult, pScriptResultLen);
  zsock_send(pub, "sss", "contact-emv", "issuer-script-result", buf);
  free(buf);
  (void) wait_for_continue_or_abort();
}

bool is_emv_card_present() {
  BYTE status;
  USHORT res = CTOS_SCStatus(d_SC_USER, &status);
  return res == d_OK && (status & d_MK_SC_PRESENT);
}

void emv_service(zsock_t *pipe, void *arg) {
  (void) arg;
  zpoller_t *poller = zpoller_new(pipe, NULL);
  EMV_EVENT emv_callback_index;
  pub = zsock_new_pub(">" EVENTS_PUB_ENDPOINT);
  sub = zsock_new_sub(">" EVENTS_SUB_ENDPOINT, "contact-emv");
  sub_or_interrupt = zpoller_new(pipe, sub, NULL);
  
  // EMV callback setup
  memset(&emv_callback_index, 0, sizeof(emv_callback_index));
  emv_callback_index.Version                  = 0x01;
  emv_callback_index.OnDisplayShow            = emv_on_display_show;
  emv_callback_index.OnErrorMsg               = emv_on_error;
  emv_callback_index.OnEMVConfigActive        = emv_on_config_active;
  // emv_callback_index.OnHashVerify             = emv_on_hash_verify;
  emv_callback_index.OnTxnDataGet             = emv_on_txn_data_get;
  emv_callback_index.OnAppList                = emv_on_app_list;
  emv_callback_index.OnAppSelectedConfirm     = emv_on_app_selected_confirm;
  emv_callback_index.OnTerminalDataGet        = NULL;
  emv_callback_index.OnCAPKGet                = NULL;
  emv_callback_index.OnGetPINNotify           = emv_on_get_pin_notify;
  emv_callback_index.OnOnlinePINBlockGet      = NULL;
  emv_callback_index.OnOfflinePINBlockGet     = NULL;
  emv_callback_index.OnOfflinePINVerifyResult = emv_on_offline_pin_verify_result;
  emv_callback_index.OnTxnOnline              = emv_on_txn_online;
  emv_callback_index.OnTxnIssuerScriptResult  = emv_on_txn_issuer_script_result;
  emv_callback_index.OnTxnResult              = emv_on_txn_result;
  // emv_callback_index.OnTotalAmountGet         = emv_on_total_amount_get;
  emv_callback_index.OnExceptionFileCheck     = NULL;
  emv_callback_index.OnCAPKRevocationCheck    = NULL;
  char *filename = find_readable_file(NULL, "emv_config.xml");
  if (!filename) {
    LERROR("emv: contact: could not find readable file: emv_config.xml");
    zsock_signal(pipe, 1);
    goto cleanup;
  }
  USHORT res = EMV_Initialize(&emv_callback_index, filename);
  if (res != d_EMVAPLIB_OK) {
    LERROR("emv: contact: initialization failed: %u", res);
    zsock_signal(pipe, 2);
    goto cleanup;
  }

  LINFO("emv: contact: service initialized");
  int attempt_count = 0;
  int emv_card_was_present = 0;
  zsock_signal(pipe, 0);

  while (1) {
    void *in = zpoller_wait(poller, DELAY_MS);
    if (!in) {
      if (!zpoller_expired(poller)) {
        LWARN("emv: contact: interrupted!");
        break;
      }
    } else if (in == pipe) {
      LINFO("emv: contact: shutting down");
      break;
    }

#define max_attempts 3

    if (is_emv_card_present()) {
      if (!emv_card_was_present) {
        LTRACE("emv: contact: card was inserted");
        zsock_send(pub, "ss", "contact-emv", "card-inserted");
        emv_card_was_present = 1;
        memset(&current_txn, 0, sizeof(current_txn));
        if (wait_for_continue_or_abort()) {
          attempt_count = max_attempts;
          continue;
        }

        // having attempt count check here forces user to remove card before
        // proceeding, while moving it outside of !emv_card_was_present will
        // retry the txn immediately.
        if (attempt_count < max_attempts) {
          char selected_aid[255], selected_label[255];
          BYTE selected_aid_len = 255,  selected_label_len = 255;
          zsock_send(pub, "ssi", "contact-emv", "txn-started", max_attempts - attempt_count);
          if (wait_for_continue_or_abort()) {
            attempt_count = max_attempts;
            continue;
          }
          attempt_count++;
          res = EMV_TxnAppSelect((BYTE *) selected_aid,   &selected_aid_len,
                                 (BYTE *) selected_label, &selected_label_len);
          if (res != d_EMVAPLIB_OK) {
            log_emv_error(res);
            if (current_txn.was_aborted) {
              LINFO("emv: contact: transaction was aborted");
              attempt_count = max_attempts;
            } else {
              if (attempt_count == max_attempts) {
                // we won't be trying the txn again. Emit the txn-complete error
                // event now, so subscribers can react at the earliest possible
                // time. Note we don't do this if the txn was explicitly aborted.
                zsock_send(pub, "sssis", "contact-emv", "txn-complete", "error", "false", "", "-1", "", "false");
              }
            }
            continue;
          }

          res = EMV_TxnPerform();
          start_keypad_service();
          if (res != d_EMVAPLIB_OK) {
            log_emv_error(res);
            if (current_txn.was_aborted) {
              LINFO("emv: contact: transaction was aborted");
              attempt_count = max_attempts;
            } else {
              if (attempt_count == max_attempts) {
                // we won't be trying the txn again. Emit the txn-complete error
                // event now, so subscribers can react at the earliest possible
                // time. Note we don't do this if the txn was explicitly aborted.
                zsock_send(pub, "sssis", "contact-emv", "txn-complete", "error", "false", "", "-1", "", "false");
              }
            }
            continue;
          }

          // success; set attempt_count to a high number to prevent another txn
          // if card is still present. attempt_count won't be reset until card is
          // removed.
          attempt_count = max_attempts;
        }
      }
    } else {
      if (emv_card_was_present) {
        LTRACE("emv: contact: card was removed");
        zsock_send(pub, "ss", "contact-emv", "card-removed");
        emv_card_was_present = 0;

        if (attempt_count >= max_attempts) {
          LTRACE("emv: contact: attempt count too high, sending txn-complete as error");
          attempt_count = 0;
        }
      }
    }
  }

cleanup:
  zpoller_destroy(&sub_or_interrupt);
  zpoller_destroy(&poller);
  zsock_destroy(&sub);
  zsock_destroy(&pub);
  sub_or_interrupt = NULL;
  sub = NULL;
  pub = NULL;
  if (filename) free(filename);
  service = NULL;
  LTRACE("emv: contact: shutdown complete");
}

#else  // HAVE_CTOS

void emv_service(zsock_t *pipe, void *arg) {
  LWARN("emv: contact: not supported on this device");
  zsock_signal(pipe, 1);
}

#endif // HAVE_CTOS

int init_emv_contact_service(void) {
  if (!service) service = zactor_new(emv_service, NULL);
  (void) pub;
  (void) sub;
  (void) sub_or_interrupt;
  return 0;
}

bool is_emv_contact_service_running(void) { return !!service; }

void shutdown_emv_contact_service(void) {
  if (service) zactor_destroy(&service);
  service = NULL;
}
