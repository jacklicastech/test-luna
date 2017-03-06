#define _GNU_SOURCE
#include "config.h"
#include <unistd.h>
#include <curl/curl.h>
#include <execinfo.h>
#include <libxml/xpath.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include "services.h"
#include "ssl_locks.h"

bool ALLOW_DISABLE_SSL_VERIFICATION = true;
char cacerts_bundle[PATH_MAX];
zsock_t *bcast = NULL;

void api_server(zsock_t *pipe, void *arg) {
  zsock_t *api = zsock_new_rep("inproc://api");
  zpoller_t *poller = zpoller_new(pipe, api, NULL);
  void *in;
  zsock_signal(pipe, 0);
  LINFO("backend-test-api-server: initialized");

  while ((in = zpoller_wait(poller, -1)) != pipe) {
    char *verb, *pkey, *path, *hkey, *headers, *bkey, *body, *response;
    zmsg_t *msg = zmsg_recv(api);
    verb = zmsg_popstr(msg);
    pkey = zmsg_popstr(msg);
    path = zmsg_popstr(msg);
    hkey = zmsg_popstr(msg);
    headers = zmsg_popstr(msg);
    bkey = zmsg_popstr(msg);
    body = zmsg_popstr(msg);
    if (strstr(headers, "echo-headers")) {
      response = strdup(headers);
    } else {
      response = (char *) calloc(strlen(verb) + strlen(body) + strlen(path) + 8, sizeof(char));
      sprintf(response, "%s\n%s\n%s", verb, path, body);
    }
    zsock_send(api, "sss", "200", "Content-type: text/plain", response);
    free(verb);
    free(pkey);
    free(path);
    free(hkey);
    free(headers);
    free(bkey);
    free(body);
    free(response);
  }

  LINFO("backend-test-api-server: shutting down");
  if (api) zsock_destroy(&api);
}

#define test_defn
  char *rkey = NULL, *result = NULL, *ckey = NULL, *bkey = NULL,*body = NULL,\
       *rid = NULL, *rid2 = NULL, *rtopic = NULL, *dkey = NULL,              \
       *duration = NULL;                                                     \
  int code, bsize;
#define freeall()                                                            \
  if (rid)      free(rid);                                                   \
  if (rid2)     free(rid2);                                                  \
  if (rkey)     free(rkey);                                                  \
  if (result)   free(result);                                                \
  if (ckey)     free(ckey);                                                  \
  if (bkey)     free(bkey);                                                  \
  if (body)     free(body);                                                  \
  if (dkey)     free(dkey);                                                  \
  if (duration) free(duration);                                              \
  if (rtopic)   free(rtopic);
#define test_req(a...)                                                       \
  assert(!zsock_send(req, a));                                               \
  assert(!zsock_recv(req, "ss", &rkey, &rid));                               \
  free(rkey);                                                                \
  assert(!zsock_recv(bcast, "sssssisbss", &rtopic,&rid2,&rkey,&result,&ckey,&code,&bkey,&body,&bsize,&dkey,&duration)); \
  body = realloc(body, bsize + 1); \
  *(body + bsize) = '\0';
#define Red     "\x1b[31m"
#define Green   "\x1b[32m"
#define Regular "\x1b[0m"
#define Assert(x)                                                            \
  if (!x) { LDEBUG(Red "Assert: FAIL: %s" Regular, #x); assert(0); }         \
  else { LDEBUG(Green "Assert: %s" Regular, #x); }
#define Assert2(x, y) \
  if (!x) { LDEBUG(Red "Assert: FAIL: (%s) : \%{%s}" Regular, #x, y); assert(0); } \
  else { LDEBUG(Green "Assert: %s" Regular, #x); }

/*
 * Requests to localhost, with cert validation disabled, should hit api
 * and echo verb+path+body.
 */
void test_get_success(zsock_t *req) {
  test_defn;
  test_req("ssssss", "verb", "GET", "url", "https://localhost:44443/123", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"),     result);
  Assert(code == 200);
  Assert2(!strcmp(body,   "GET\n/123\n"), body);
  freeall();
}

/*
 * Requests to localhost, with cert validation disabled, should hit api
 * and echo verb+path+body. The verb is notably omitted so should default to
 * GET.
 */
void test_no_verb_defaults_to_get(zsock_t *req) {
  test_defn;
  test_req("ssss", "url", "https://localhost:44443/123", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"),     result);
  Assert(code == 200);
  Assert2(!strcmp(body,   "GET\n/123\n"), body);
  freeall();
}

/*
 * Requests to localhost, with cert validation disabled, should hit api
 * and echo verb+path+body. Even for GET requests a body should be sent if
 * provided.
 */
void test_get_with_body(zsock_t *req) {
  test_defn;
  test_req("ssssssss", "verb", "GET", "url", "https://localhost:44443/123", "body", "this is a body", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), result);
  Assert(code == 200);
  Assert2(!strcmp(body, "GET\n/123\nthis is a body"), body);
  freeall();
}

/*
 * Requests to localhost, with cert validation disabled, should hit api
 * and echo verb+path+body. Test POST request with body.
 */
void test_post_with_body(zsock_t *req) {
  test_defn;
  test_req("ssssssss", "verb", "POST", "url", "https://localhost:44443/123", "body", "this is a body", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), result);
  Assert(code == 200);
  Assert2(!strcmp(body, "POST\n/123\nthis is a body"), body);
  freeall();
}

/*
 * Of course the SSL cert for localhost is invalid, so if we enable cert
 * validation, an otherwise valid GET should fail.
 */
void test_get_certfail(zsock_t *req) {
  test_defn;
  test_req("ssss", "verb", "GET", "url", "https://localhost:44443/123");
  Assert2(!strcmp(result, "error"),       result);
  Assert2( strcmp(body,   "GET\n/123\n"), body);
  freeall();
}

/*
 * Of course the SSL cert for localhost is invalid, so if we enable cert
 * validation, an otherwise valid GET should fail.
 */
void test_malformed_no_url(zsock_t *req) {
  test_defn;
  test_req("ss", "verb", "GET");
  Assert2(!strcmp(result, "error"),       result);
  Assert2( strcmp(body,   "GET\n/123\n"), body);
  freeall();
}

/*
 * All HTTP requests should be rejected because only SSL connections will be
 * allowed. This prevents us from sending sensitive data in the open.
 *
 * Note: the domain is in the whitelist so over SSL this would be OK.
 */
void test_get_nossl_fails(zsock_t *req) {
  test_defn;
  test_req("ssss", "verb", "GET", "url", "http://www.google.com/");
  Assert2(!strcmp(result, "error"), body);
  freeall();
}

/*
 * The inverse of `test_get_nossl_fails`, this is a sanity check to
 * make sure an SSL connection to the same host can be established. In other
 * words, this is a test of the test environment, to make sure the domain is
 * really whitelisted and otherwise reachable.
 */
void test_get_nossl_sanity_check(zsock_t *req) {
  test_defn;
  test_req("ssss", "verb", "GET", "url", "https://www.google.com/");
  Assert2(!strcmp(result, "success"), body);
  freeall();
}

/*
 * Make sure that headers, if specified, get sent as part of the request.
 * Our little API service will echo headers instead of request parts, if it
 * sees the 'echo-headers' header.
 */
void test_headers_dispatched(zsock_t *req) {
  test_defn;
  test_req("ssssssssss", "url", "https://localhost:44443/", "echo-headers", "true", "content-type", "application/json", "body", "{}", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), result);
  Assert2(strstr(body, "content-type: application/json"), body);
  freeall();
}

/*
 * Make sure that URLs containing token references get serialized into the
 * token data itself.
 */
void test_valid_token_in_body(zsock_t *req) {
  test_defn;
  char tok_data[] = { '1', '2', '3', '4', '\0' };
  zsock_t *tok_sock = zsock_new_req(TOKENS_ENDPOINT);
  token_id tok_id;
  char req_data[1024];

  // convert our 'sensitive' data into a token
  assert(!zsock_send(tok_sock, "bs", tok_data, sizeof(tok_data), "tokdata"));
  assert(!zsock_recv(tok_sock, "u", &tok_id));
  zsock_destroy(&tok_sock);

  // send a request including a reference to the token
  sprintf(req_data, "token = :%s%u%s:", TOKEN_PREFIX, tok_id, TOKEN_SUFFIX);
  test_req("ssssssss", "verb", "POST", "url", "https://localhost:44443/", "body", req_data, "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), result);
  Assert2(!strcmp(body, "POST\n/\ntoken = :1234:"), body);

  free_token(tok_id);
  freeall();
}

/*
 * Make sure that headers containing token references get serialized into the
 * token data itself.
 */
void test_valid_token_in_headers(zsock_t *req) {
  test_defn;
  char tok_data[] = { '1', '2', '3', '4', '\0' };
  zsock_t *tok_sock = zsock_new_req(TOKENS_ENDPOINT);
  token_id tok_id;
  char req_data[1024];

  // convert our 'sensitive' data into a token
  assert(!zsock_send(tok_sock, "bs", tok_data, sizeof(tok_data), "tokdata"));
  assert(!zsock_recv(tok_sock, "u", &tok_id));
  zsock_destroy(&tok_sock);

  // send a request including a reference to the token
  sprintf(req_data, "token = :%s%u%s:", TOKEN_PREFIX, tok_id, TOKEN_SUFFIX);
  test_req("ssssssssss", "verb", "POST", "url", "https://localhost:44443/", "token", req_data, "echo-headers", "true", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), result);
  Assert2(strstr(body, "token = :1234:"), body);

  free_token(tok_id);
  freeall();
}

/*
 * Make sure that body containing token references gets serialized into the
 * token data itself.
 */
void test_valid_token_in_url(zsock_t *req) {
  test_defn;
  char tok_data[] = { '1', '2', '3', '4', '\0' };
  zsock_t *tok_sock = zsock_new_req(TOKENS_ENDPOINT);
  token_id tok_id;
  char req_data[1024];

  // convert our 'sensitive' data into a token
  assert(!zsock_send(tok_sock, "bs", tok_data, sizeof(tok_data), "tokdata"));
  assert(!zsock_recv(tok_sock, "u", &tok_id));
  zsock_destroy(&tok_sock);

  // send a request including a reference to the token
  sprintf(req_data, "https://localhost:44443/txn/%s%u%s/", TOKEN_PREFIX, tok_id, TOKEN_SUFFIX);

  test_req("ssssssss", "verb", "POST", "url", req_data, "body", "", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), body);
  Assert2(!strcmp(body, "POST\n/txn/1234/\n"), body);

  free_token(tok_id);
  freeall();
}

/*
 * Make sure that body containing invalid token references gets serialized
 * into an empty string. Note that this id might even match an id previously
 * freed.
 */
void test_invalid_token_in_body(zsock_t *req) {
  test_defn;
  char tok_data[] = { '1', '2', '3', '4', '\0' };
  zsock_t *tok_sock = zsock_new_req(TOKENS_ENDPOINT);
  token_id tok_id;
  char req_data[1024];

  // convert our 'sensitive' data into a token
  assert(!zsock_send(tok_sock, "bs", tok_data, sizeof(tok_data), "tokdata"));
  assert(!zsock_recv(tok_sock, "u", &tok_id));
  zsock_destroy(&tok_sock);

  // now free the token, to ensure that we have a legitimately invalid token
  // id
  free_token(tok_id);

  // send a request including a reference to the token
  sprintf(req_data, "token = :%s%u%s:", TOKEN_PREFIX, tok_id, TOKEN_SUFFIX);
  test_req("ssssssss", "verb", "POST", "url", "https://localhost:44443/", "body", req_data, "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), result);
  Assert2(!strcmp(body, "POST\n/\ntoken = ::"), body);

  freeall();
}

/*
 * Make sure that sending a POST with a body, and then following it with a
 * GET with no body, doesn't resend the POST's body. This could be an artifact
 * of reusing resources.
 */
void test_reuse_resources_without_leaking_data(zsock_t *req) {
  test_defn;
  test_req("ssssssss", "verb", "POST", "url", "https://localhost:44443/", "body", "hello", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), result);
  Assert2(!strcmp(body, "POST\n/\nhello"), body);
  printf("%s\n", body);
  freeall();

  test_req("ssssss", "verb", "GET", "url", "https://localhost:44443/", "validate_ssl_certificates", "false");
  Assert2(!strcmp(result, "success"), result);
  Assert2(!strcmp(body, "GET\n/\n"), body);
  printf("%s\n", body);
  freeall();
}

void termination_handler(int signum) {
  LERROR("received signal %d", signum);

  void *array[20];
  size_t size;
  size = backtrace(array, 20);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

int main(int argc, char **argv) {
  int err = 0;
  char *cwd = get_current_dir_name();
  zactor_t *api = NULL;

  if (signal(SIGABRT, termination_handler) == SIG_IGN) signal(SIGABRT, SIG_IGN);

  curl_global_init(CURL_GLOBAL_ALL);
  xmlInitParser();
  sprintf(cacerts_bundle, "%s/fs_data/cacerts.pem", cwd);
  free(cwd);

  init_ssl_locks();
  if ((err = init_logger_service(LOG_LEVEL_INSEC))) goto shutdown;
  if ((err = init_tokenizer_service()))             goto shutdown;
  if ((err = init_settings_service()))              goto shutdown;
  if ((err = init_webserver_service()))             goto shutdown;
  if ((err = init_events_proxy_service()))          goto shutdown;
  if ((err = init_plugins(NULL, argc, argv)))       goto shutdown;

  zsock_t *req = zsock_new_req("inproc://backend");
  bcast = zsock_new_sub(">" EVENTS_SUB_ENDPOINT, "backend-complete");
  assert(bcast);
#define T(x) printf(#x "\n"); x;

  assert(api = zactor_new(api_server, NULL));
  // test certfail first, because testing success first can cause curl to
  // reuse the open connection as part of its auto keep-alive.
  T(test_get_certfail(req));
  T(test_get_success(req));
  T(test_get_nossl_fails(req));
  T(test_get_nossl_sanity_check(req));
  T(test_malformed_no_url(req));
  T(test_no_verb_defaults_to_get(req));
  T(test_post_with_body(req));
  T(test_get_with_body(req));
  T(test_headers_dispatched(req));
  T(test_valid_token_in_body(req));
  T(test_invalid_token_in_body(req));
  T(test_valid_token_in_url(req));
  T(test_valid_token_in_headers(req));
  T(test_reuse_resources_without_leaking_data(req));
  zactor_destroy(&api);
  
shutdown:
  if (bcast) zsock_destroy(&bcast);
  if (req)   zsock_destroy(&req);
  shutdown_plugins();
  shutdown_events_proxy_service();
  shutdown_webserver_service();
  shutdown_settings_service();
  shutdown_tokenizer_service();
  shutdown_logger_service();
  shutdown_ssl_locks();
  xmlCleanupParser();
  curl_global_cleanup();

  return err;
}
