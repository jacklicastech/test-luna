/* 
 * File:   transactions_service.h
 * Author: Colin
 *
 * Created on December 18, 2015, 2:26 AM
 */

#ifndef TRANSACTIONS_SERVICE_H
#define	TRANSACTIONS_SERVICE_H

#include "AppBatch.h"
#include "czmq.h"
#include "util/uthash.h"
    
#define TRANSACTIONS_ENDPOINT "inproc://transactions"

#ifdef __cplusplus
extern "C" {
#endif

int init_transactions_service(void);
void shutdown_transactions_service(void);

#ifdef __cplusplus
}
#endif

/*
 * The service running at TRANSACTIONS_ENDPOINT is a request/reply service.
 * Consumers should perform an initial request to the endpoint. The request
 * format can take one of four forms:
 * 
 *      1. The first frame contains a PERFORM value and the second frame
 *         contains a transaction_request_t, indicating the consumer wishes to
 *         perform a new transaction.
 * 
 *      2. The first frame contains a QUERY value and the second frame contains
 *         the transaction ID (a string), indicating the consumer wishes to
 *         check the status of an existing transaction.
 * 
 *      3. The first frame contains a LIST value. Subsequent frames, if any, are
 *         ignored. The server will return all transactions recently performed.
 * 
 *      4. The first frame contains a FLUSH value. Subsequent frames, if any,
 *         are ignored. The server will remove all transactions and clear the
 *         current batch. There will be no server-side record of the
 *         transactions.
 * 
 * In all cases, a response will be sent immediately. If a transaction is
 * already in progress, PERFORM requests will receive a single BUSY frame.
 * Otherwise it will receive a single ENQUEUED frame. QUERY requests will
 * always receive only a transaction_summary_t frame, but it may point to a NULL
 * value if the transaction could not be found. LIST requests will receive
 * a transaction_summary_t frame for each transaction record to be returned.
 * FLUSH requests will receive a single OK frame.
 */

#define TXN_REQUEST_QUERY              0
#define TXN_REQUEST_PERFORM            1
#define TXN_REQUEST_LIST               2
#define TXN_REQUEST_FLUSH              3
#define TXN_RESPONSE_BUSY              4
#define TXN_RESPONSE_ENQUEUED          5
#define TXN_RESPONSE_DUPLICATE_ID      6
#define TXN_RESPONSE_ERROR_BAD_REQUEST 7
#define TXN_RESPONSE_OK                8

#define TYPE_SALE            0
#define TYPE_REFUND          1
#define TYPE_VERIFICATION    2
#define TYPE_BALANCE_INQUIRY 3

#define STATUS_IN_PROGRESS        0
#define STATUS_APPROVED           1
#define STATUS_PARTIALLY_APPROVED 2
#define STATUS_DECLINED           3

typedef struct {
    int type;
    long amount;
    char *id;
} transaction_request_t;

typedef struct {
    char *id;
    int type;
    int status;
    long requested_amount;
    long authorized_amount;
    char *authorization_code;
    char *avs_result;
    char *cvv_result;
    char *card_type;
    char *masked_card_number;
    BatchRecord *batch_record;
    
    UT_hash_handle hh;
    
    zactor_t *_actor;
} transaction_summary_t;

#endif	/* TRANSACTIONS_SERVICE_H */

