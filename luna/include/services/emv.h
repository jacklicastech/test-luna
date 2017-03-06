#ifndef SERVICES_EMV_H
#define SERVICES_EMV_H

#ifdef  __cplusplus
extern "C" {
#endif
  #define EMV_ENDPOINT             "inproc://emv"
  #define EMV_CONTACTLESS_ENDPOINT "inproc://emv-contactless"

  // http://www.emvlab.org/emvtags/all/

  // Source: terminal
  #define EMV_TAG_ACQUIRER_ID                                           0x9F01
  #define EMV_TAG_ADDL_TERM_CAPS                                        0x9F40
  #define EMV_TAG_AUTHORIZED_AMT_BIN                                    0x0081
  #define EMV_TAG_AUTHORIZED_AMT_ASCII                                  0x9F02
  #define EMV_TAG_CASHBACK_AMT_BIN                                      0x9F04
  #define EMV_TAG_CASHBACK_AMT_ASCII                                    0x9F03
  #define EMV_TAG_AUTHORIZED_AMT_REFERENCE_CURRENCY                     0x9F3A
  #define EMV_TAG_APP_ID_TERMINAL                                       0x9F06
  #define EMV_TAG_APP_VERSION_NUMBER                                    0x9F08
  #define EMV_TAG_TERMINAL_APP_VERSION_NUMBER                           0x9F09
  #define EMV_TAG_CARDHOLDER_VERIFICATION_METHOD_RESULTS                0x9F34
  #define EMV_TAG_CERTIFICATION_AUTHORITY_PUBLIC_KEY_INDEX              0x008F
  #define EMV_TAG_COMMAND_TEMPLATE                                      0x0083
  #define EMV_TAG_INTERFACE_DEVICE_SERIAL_NUMBER                        0x9F1E
  #define EMV_TAG_MERCHANT_CATEGORY_CODE                                0x9F15
  #define EMV_TAG_MERCHANT_ID                                           0x9F16
  #define EMV_TAG_MERCHANT_NAME_AND_LOCATION                            0x9F4E
  #define EMV_TAG_POS_ENTRY_MODE                                        0x9F39
  #define EMV_TAG_TERMINAL_CAPABILITIES                                 0x9F33
  #define EMV_TAG_TERMINAL_COUNTRY_CODE                                 0x9F1A
  #define EMV_TAG_TERMINAL_FLOOR_LIMIT                                  0x9F1B
  #define EMV_TAG_TERMINAL_ID                                           0x9F1C
  #define EMV_TAG_TERMINAL_RISK_MANAGEMENT_DATA                         0x9F1D
  #define EMV_TAG_TERMINAL_TYPE                                         0x9F35
  #define EMV_TAG_TERMINAL_VERIFICATION_RESULTS                         0x0095
  #define EMV_TAG_TRANSACTION_CERTIFICATE_HASH                          0x0098
  #define EMV_TAG_TRANSACTION_CURRENCY_CODE                             0x5F2A
  #define EMV_TAG_TRANSACTION_CURRENCY_EXPONENT                         0x5F36
  #define EMV_TAG_TRANSACTION_DATE                                      0x009A
  #define EMV_TAG_TRANSACTION_PIN_DATA                                  0x0099
  #define EMV_TAG_TRANSACTION_REFERENCE_CURRENCY_CODE                   0x9F3C
  #define EMV_TAG_TRANSACTION_REFERENCE_CURRENCY_EXPONENT               0x9F3D
  #define EMV_TAG_TRANSACTION_SEQUENCE_COUNTER                          0x9F41
  #define EMV_TAG_TRANSACTION_STATUS_INFORMATION                        0x009B
  #define EMV_TAG_TRANSACTION_TIME                                      0x9F21
  #define EMV_TAG_TRANSACTION_TYPE                                      0x009C
  #define EMV_TAG_UNPREDICTABLE_NUMBER                                  0x9F37
  #define EMV_TAG_TERMINAL_ACTION_CODE_DENIAL                           0xDFC6
  #define EMV_TAG_TERMINAL_ACTION_CODE_ONLINE                           0xDFC7
  #define EMV_TAG_TERMINAL_ACTION_CODE_DEFAULT                          0xDFC8

  // Source: ICC
  #define EMV_TAG_APP_CRYPTOGRAM                                        0x9F26
  #define EMV_TAG_APP_CURRENCY_CODE                                     0x9F42
  #define EMV_TAG_APP_CURRENCY_EXPONENT                                 0x9F44
  #define EMV_TAG_APP_DISCRETIONARY_DATA                                0x9F05
  #define EMV_TAG_APP_EFFECTIVE_DATE                                    0x5F25
  #define EMV_TAG_APP_EXPIRATION_DATE                                   0x5F24
  #define EMV_TAG_APP_FILE_LOCATOR                                      0x0094
  #define EMV_TAG_APP_ID_CARD                                           0x004F
  #define EMV_TAG_APP_INTERCHANGE_PROFILE                               0x0082
  #define EMV_TAG_APP_LABEL                                             0x0050
  #define EMV_TAG_APP_PREFERRED_NAME                                    0x9F12
  #define EMV_TAG_APP_PRIMARY_ACCT_NUMBER                               0x005A
  #define EMV_TAG_APP_PRIMARY_ACCT_NUMBER_SEQ_NUMBER                    0x5F34
  #define EMV_TAG_APP_PRIORITY_INDICATOR                                0x0087
  #define EMV_TAG_APP_REFERENCE_CURRENCY_CODES                          0x9F3B
  #define EMV_TAG_APP_REFERENCE_CURRENCY_EXPONENTS                      0x9F43
  #define EMV_TAG_APP_TEMPLATE                                          0x0061
  #define EMV_TAG_APP_TRANSACTION_COUNTER                               0x9F36
  #define EMV_TAG_APP_USAGE_CONTROL                                     0x9F07
  #define EMV_TAG_BANK_ID_CODE                                          0x5F54
  #define EMV_TAG_CARD_RISK_MGMT_DATA_OBJECT_1                          0x008C
  #define EMV_TAG_CARD_RISK_MGMT_DATA_OBJECT_2                          0x008D
  #define EMV_TAG_CARDHOLDER_NAME                                       0x5F20
  #define EMV_TAG_CARDHOLDER_NAME_EXTENDED                              0x9F0B
  #define EMV_TAG_CARDHOLDER_VERIFICATION_METHOD_LIST                   0x008E
  #define EMV_TAG_CRYPTOGRAM_INFORMATION_DATA                           0x9F27
  #define EMV_TAG_DATA_AUTHENTICATION_CODE                              0x9F45
  #define EMV_TAG_DEDICATED_FILE_NAME                                   0x0084
  #define EMV_TAG_DIRECTORY_DEFINITION_FILE_NAME                        0x009D
  #define EMV_TAG_DIRECTORY_DISCRETIONARY_TEMPLATE                      0x0073
  #define EMV_TAG_DYNAMIC_DATA_AUTHENTICATION_DATA_OBJECT_LIST          0x9F49
  #define EMV_TAG_EMV_PROPRIETARY_TEMPLATE                              0x0070
  #define EMV_TAG_FILE_CONTROL_INFO_ISSUER_DISCRETIONARY_DATA           0xBF0C
  #define EMV_TAG_FILE_CONTROL_INFO_PROPRIETARY_TEMPLATE                0x00A5
  #define EMV_TAG_FILE_CONTROL_INFO_TEMPLATE                            0x006F
  #define EMV_TAG_ICC_DYNAMIC_NUMBER                                    0x9F4C
  #define EMV_TAG_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_CERT                  0x9F2D
  #define EMV_TAG_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_EXPONENT              0x9F2E
  #define EMV_TAG_ICC_PIN_ENCIPHERMENT_PUBLIC_KEY_REMAINDER             0x9F2F
  #define EMV_TAG_ICC_PUBLIC_KEY_CERTIFICATE                            0x9F46
  #define EMV_TAG_ICC_PUBLIC_KEY_EXPONENT                               0x9F47
  #define EMV_TAG_ICC_PUBLIC_KEY_REMAINDER                              0x9F48
  #define EMV_TAG_INTERNATIONAL_BANK_ACCOUNT_NUMBER                     0x5F53
  #define EMV_TAG_ISSUER_ACTION_CODE_DEFAULT                            0x9F0D
  #define EMV_TAG_ISSUER_ACTION_CODE_DENIAL                             0x9F0E
  #define EMV_TAG_ISSUER_ACTION_CODE_ONLINE                             0x9F0F
  #define EMV_TAG_ISSUER_APPLICATION_DATA                               0x9F10
  #define EMV_TAG_ISSUER_CODE_TABLE_INDEX                               0x9F11
  #define EMV_TAG_ISSUER_COUNTRY_CODE                                   0x5F28
  #define EMV_TAG_ISSUER_COUNTRY_CODE_ALPHA2                            0x5F55
  #define EMV_TAG_ISSUER_COUNTRY_CODE_ALPHA3                            0x5F56
  #define EMV_TAG_ISSUER_ID_NUMBER                                      0x0042
  #define EMV_TAG_ISSUER_PUBLIC_KEY_CERTIFICATE                         0x0090
  #define EMV_TAG_ISSUER_PUBLIC_KEY_EXPONENT                            0x9F32
  #define EMV_TAG_ISSUER_PUBLIC_KEY_REMAINDER                           0x0092
  #define EMV_TAG_ISSUER_URL                                            0x5F50
  #define EMV_TAG_LANGUAGE_PREFERENCE                                   0x5F2D
  #define EMV_TAG_LAST_ONLINE_APP_TRANSACTION_COUNTER_REGISTER          0x9F13
  #define EMV_TAG_LOG_ENTRY                                             0x9F4D
  #define EMV_TAG_LOG_FORMAT                                            0x9F4F
  #define EMV_TAG_LOWER_CONSECUTIVE_OFFLINE_LIMIT                       0x9F14
  #define EMV_TAG_PIN_ATTEMPTS_REMAINING                                0x9F17
  #define EMV_TAG_PROCESSING_OPTIONS_DATA_OBJECT_LIST                   0x9F38
  #define EMV_TAG_RESPONSE_MESSAGE_TEMPLATE_FORMAT1                     0x0080
  #define EMV_TAG_RESPONSE_MESSAGE_TEMPLATE_FORMAT2                     0x0077
  #define EMV_TAG_SERVICE_CODE                                          0x5F30
  #define EMV_TAG_SHORT_FILE_ID                                         0x0088
  #define EMV_TAG_SIGNED_DYNAMIC_APP_DATA                               0x9F4B
  #define EMV_TAG_SIGNED_STATIC_APP_DATA                                0x0093
  #define EMV_TAG_STATIC_DATA_AUTHENTICATION_LIST                       0x9F4A
  #define EMV_TAG_TRACK1_DISCRETIONARY_DATA                             0x9F1F
  #define EMV_TAG_TRACK2_DISCRETIONARY_DATA                             0x9F20
  #define EMV_TAG_TRACK2_EQUIVALENT_DATA                                0x0057
  #define EMV_TAG_TRANSACTION_CERTIFICATE_DATA_OBJECT_LIST              0x0097
  #define EMV_TAG_UPPER_CONSECUTIVE_OFFLINE_LIMIT                       0x9F23
  #define EMV_TAG_TRANSACTION_CATEGORY_CODE                             0x9F53

  // Source: Issuer
  #define EMV_TAG_AUTHORIZATION_CODE                                    0x0089
  #define EMV_TAG_AUTHORIZATION_RESPONSE_CODE                           0x008A
  #define EMV_TAG_ISSUER_AUTHENTICATION_DATA                            0x0091
  #define EMV_TAG_ISSUER_SCRIPT_COMMAND                                 0x0086
  #define EMV_TAG_ISSUER_SCRIPT_ID                                      0x9F18
  #define EMV_TAG_ISSUER_SCRIPT_TEMPLATE1                               0x0071
  #define EMV_TAG_ISSUER_SCRIPT_TEMPLATE2                               0x0072

  #include "util/emv_helpers.h"

  /*
   * Receives a blob of arbitrary binary data and its length, and a string
   * to prefix log entries with, and dumps its content in hex format to the
   * INSECURE log.
   */
  void dump_blob_as_hex(unsigned char *data, size_t len, const char *log_prefix);

  int  init_emv_contactless_service(void);
  void shutdown_emv_contactless_service(void);

#ifdef __cplusplus
}
#endif

#endif // SERVICES_EMV_H
