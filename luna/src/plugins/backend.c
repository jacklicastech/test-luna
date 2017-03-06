/**
 * Performs HTTPS requests to a specified backend server. Make a new request
 * by sending a message to "inproc://backend". After sending your request,
 * wait for a response which will contain the request ID. Subscribe to
 * "inproc://events/sub" to receive the result when it completes. Errors will
 * be broadcast with the same request ID and format as a successful result.
 *
 * SSL certificate verification is normally always enabled, except in special
 * debug builds.
 *
 * The domain name of the HTTP request must appear in the whitelist file at
 * 'whitelist.txt'. The whitelist file contains one domain name per line, with
 * no other data. Wildcards are not supported in this file. Any request to a
 * domain name used which is not present in the whitelist will fail
 * immediately.
 *
 * HTTP without SSL is not supported and will fail immediately.
 *
 *
 * ## Making a request
 *
 * To initiate a new request, send a message to a request socket connecting to
 * "inproc://backend" with the following format:
 *
 *     ["[key1]", "[value1]", "[key2]", "[value2]", ...]
 *
 * The number of frames must be even, with frame 1 being the first frame.
 *
 * Each odd-numbered frame is a key, defining what type of value will follow.
 * Keys can include:
 *
 * * "url" - the value will be a fully-qualified URL. If this is a GET
 *   request, the value should include the query string, if any. Mandatory.
 *
 * * "method" or "verb" - both "method" and "verb" refer to the same thing.
 *   The value must be one of the HTTP verbs, such as "GET", "POST", "PUT" or
 *   "DELETE". If omitted, the default method is "GET".
 *
 * * "body" - the value will be the body of the HTTP request. If omitted,
 *   the request has no body.
 *
 * * "validate_ssl_certificates" - the value will be "true" to indicate that
 *   SSL certificate validation is required. The value will be "false" to
 *   indicate that SSL certificate validation should be suppressed, if this
 *   software supports the request. If it is unsupported, an error will be
 *   logged and SSL validation will take place. The default is "true".
 *
 * * any other key - If not one of the above values, the key refers to an HTTP
 *   header of the same name, and the value will be the value to be given to
 *   the HTTP header. For example, pass the "Content-type" key with the
 *   "application/json" value to indicate that the request "body" will be in
 *   JSON format.
 *
 *
 * ## Request ID
 *
 * When you send the request data, you must receive a response on the same
 * socket. The response message contains the following format:
 *
 *     ["broadcast_id", "[id]"]
 *
 * The first frame is the literal string value "broadcast_id".
 *
 * The second frame contains the request ID to which the result will be
 * broadcast, on the "inproc://events/sub" channel, with the topic
 * "backend-complete".
 *
 *
 * ## Result Broadcast
 *
 * When the request errors or completes, the result will be published to the
 * "inproc://events/sub" channel with the following format:
 *
 *     ["backend-complete", "[id]", "result", "[result]", "code", "[code]",
 *      "body", "[body]", "duration", "[duration]"]
 *
 * The first frame contains the topic name and is always "backend-complete".
 *
 * The second frame contains the request ID which you received as a response
 * to your initial request.
 *
 * The remaining frames are in key-value notation, with each odd-numbered
 * frame starting with the third being the key and the even-numbered frame
 * following it containing the corresponding value.
 *
 * The "result" key will be either "error" to indicate the request could not
 * be completed, or "success" to indicate that it was completed. Note that
 * the value "success" does not necessarily indicate an HTTP success (e.g.
 * code 20x).
 * 
 * The "code" key will contain the HTTP status code, such as "200", "201",
 * or "404".
 *
 * The "body" key will contain the HTTP response body, if any. Even if there
 * was no HTTP response body, the key will be present. In that case the value
 * for this key will be an empty string.
 *
 * The "duration" key will contain the total duration of the HTTP request, in
 * seconds, encoded as a string containing a floating-point value.
 *
 **/

#define _GNU_SOURCE
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <pthread.h>
#include <uriparser/Uri.h>
#include "services.h"
#include "util/curl_utils.h"
#include "util/detokenize_template.h"
#include "util/files.h"

extern bool ALLOW_DISABLE_SSL_VERIFICATION;

typedef struct {
  char *hostname;
  UT_hash_handle hh;
} whitelist_entry_t;

typedef struct {
  char id[64];
  zmsg_t *params;
} request_t;

// The whitelist is loaded once at startup, and is immutable. It is shared
// amongst all background processors. I think this is safe because it never
// changes. If I'm wrong we can introduce some overhead to copy the whitelist
// for each background processor, but that seems unnecessary and wasteful.
static whitelist_entry_t *whitelist = NULL;

// The main backend service.
static zactor_t          *service   = NULL;

/*
 * Returns 1 if the hostname appears in the whitelist, 0 otherwise.
 */
int is_whitelisted(const char *hostname, const size_t len) {
  whitelist_entry_t *entry = NULL;
  HASH_FIND(hh, whitelist, hostname, len, entry);
  if (entry == NULL) return 0;
  return 1;
}

void perform_request(int worker_id, const char *request_id, CURL *curl, zmsg_t *msg, zsock_t *pipe) {
  UriParserStateA state;
  UriUriA uri;
  int i, len;
  char *request_url          = NULL;
  char *request_method       = NULL;
  char *request_body         = NULL;
  struct MemoryStruct response_data;
  struct curl_slist *request_headers = NULL;
  CURLcode res;
  struct {
    char status[64];
    int code;
    char *body;
    char duration[64];
  } result;
  memset(&result, 0, sizeof(result));
  size_t val_len, request_body_len = 0;

  memset(&response_data, 0, sizeof(response_data));
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);

  len = zmsg_size(msg);
  LINFO("backend: worker %d: %s: processing request %d parts", worker_id, request_id, len);
  for (i = 0; i < len; i += 2) {
    char *key = zmsg_popstr(msg);
    char *val = zmsg_popstr(msg);
    val_len = strlen(val);
    LTRACE("backend: worker %d: %s: processing key %s", worker_id, request_id, key);
    if (!strcmp(key, "url")) {
      request_url = detokenize_template(val, &val_len);
      free(val);
      LINSEC("backend: worker %d: %s: request url: %s", worker_id, request_id, request_url);
    } else if (!strcmp(key, "method") || !strcmp(key, "verb")) {
      request_method = val;
      LDEBUG("backend: worker %d: %s: request method: %s", worker_id, request_id, request_method);
    } else if (!strcmp(key, "body")) {
      request_body_len = val_len;
      request_body = strdup(val);
      free(val);
    } else if (!strcmp(key, "validate_ssl_certificates")) {
      if (!strcmp(val, "true") || !strcmp(val, "yes")) {
        LINFO("backend: worker %d: %s: SSL verification enabled", worker_id, request_id);
      } else {
        if (ALLOW_DISABLE_SSL_VERIFICATION) {
          LWARN("backend: worker %d: %s: NOT performing SSL certificate validation!", worker_id, request_id);
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
          curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        } else {
          LERROR("backend: worker %d: %s: SSL verification cannot be disabled", worker_id, request_id);
        }
      }
      free(val);
    } else {
      char *detokenized_val = detokenize_template(val, &val_len);
      free(val);
      val = detokenized_val;
      char *tmp = NULL;
      if (asprintf(&tmp, "%s: %s", key, val)) {}
      request_headers = curl_slist_append(request_headers, tmp);
      LINSEC("backend: worker %d: %s: request header: %s", worker_id, request_id, tmp);
      free(val);
      free(tmp);
    }
    free(key);
  }

  if (!request_method) request_method = strdup("GET");

#define RESULT(_status, _code, _body, _duration)             \
        result.code = _code;                                 \
        if (strlen(_body) > 0) result.body = strdup(_body);  \
        sprintf(result.status,   "%.*s", 63, _status);       \
        sprintf(result.duration, "%.*s", 63, _duration);
  if (request_url) {
    curl_easy_setopt(curl, CURLOPT_URL, request_url);
    state.uri = &uri;

    if (uriParseUriA(&state, request_url) != URI_SUCCESS) {
      RESULT("error", -1, "could not parse your URL", "0.0");
    } else if (uriNormalizeSyntaxExA(&uri, URI_NORMALIZE_SCHEME | URI_NORMALIZE_HOST) != URI_SUCCESS) {
      RESULT("error", -2, "could not normalize your URL", "0.0");
    } else {
      const char *host = uri.hostText.first;
      char scheme[20];
      double total_duration = 0.0;
      char total_duration_str[3 + DBL_MANT_DIG - DBL_MIN_EXP + 1];
      int schemelen = uri.scheme.afterLast - uri.scheme.first;
      if (schemelen > 19) schemelen = 19;
      memcpy(scheme, uri.scheme.first, schemelen);
      scheme[uri.scheme.afterLast - uri.scheme.first] = '\0';

      if (strcmp(scheme, "https")) {
        RESULT("error", -3, "only HTTPS URLs are allowed", "0.0");
      } else {
        if (is_whitelisted(host, (int) (uri.hostText.afterLast - uri.hostText.first))) {
          LDEBUG("backend: worker %d: %s: URL is whitelisted, sensitive data will be allowed", worker_id, request_id);
          if (request_body) {
            char *tmp = detokenize_template(request_body, &request_body_len);
            free(request_body);
            request_body = tmp;
            LINSEC("backend: worker %d: %s: request body (%lld bytes): %s", worker_id, request_id, (long long) request_body_len, request_body);
          }
        } else {
          LDEBUG("backend: worker %d: %s: URL is NOT whitelisted, sensitive data will be disallowed", worker_id, request_id);
          if (request_body) {
            char *tmp = humanize_template(request_body, &request_body_len);
            free(request_body);
            request_body = tmp;
            LINSEC("backend: worker %d: %s: request body (%lld bytes): %s", worker_id, request_id, (long long) request_body_len, request_body);
          }
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    request_headers);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request_method);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    request_body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, request_body_len);
        curl_easy_setopt(curl, CURLOPT_CAINFO,        cacerts_bundle);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_cb_accum_mem);

        res = curl_easy_perform(curl);
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_duration);
        sprintf(total_duration_str, "%f", total_duration);
        if(res != CURLE_OK) {
          LERROR("backend: worker %d: %s: request failed: %s (%s secs)", worker_id, request_id, curl_easy_strerror(res), total_duration_str);
          RESULT("error", res, curl_easy_strerror(res), total_duration_str);
        } else {
          long http_code = 0;
          curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
          LDEBUG("backend: worker %d: %s: request succeeded: %ld in %s ms", worker_id, request_id, http_code, total_duration_str);
          int i, printable = 1;
          for (i = 0; i < response_data.size; i++)
            if (!isprint(*(response_data.memory + i)) && *(response_data.memory + i) != '\n' && *(response_data.memory + i) != '\r')
              printable = 0;
          if (printable && response_data.size)
            LINSEC("backend: worker %d: %s: response body (cstr): %.*s", worker_id, request_id, response_data.size, response_data.memory);
          else {
            LINSEC("backend: worker %d: %s: response body (blob): %zu bytes", worker_id, request_id, response_data.size);
            char prefix[1024];
            sprintf(prefix, "backend: worker %d: %s: response body (blob):", worker_id, request_id);
            // dump_blob_as_hex((unsigned char *) response_data.memory, response_data.size, prefix);
          }
          RESULT("success", http_code, "", total_duration_str);
          result.body = NULL;
        }
      }
    }

    uriFreeUriMembersA(&uri);
  } else {
    RESULT("error", -5, "you did not specify a URL", "0.0");
  }

  if (!response_data.memory) {
    if (result.body) {
      response_data.memory = result.body;
      response_data.size = strlen(result.body);
      result.body = NULL;
    } else {
      response_data.memory = strdup("");
      response_data.size = 1;
    }
  }
  assert(result.body == NULL);
  assert(!zsock_send(pipe, "sssssisbss", "backend-complete", request_id,
                                         "result",   result.status,
                                         "code",     result.code,
                                         "body",     response_data.memory, response_data.size,
                                         "duration", result.duration));
  LTRACE("backend: worker %d: %s: sent result", worker_id, request_id);

  if (response_data.memory) free(response_data.memory);
  free(request_url);
  free(request_method);
  free(request_body);
  curl_slist_free_all(request_headers);
  LDEBUG("backend: worker %d: %s: request completed", worker_id, request_id);
}

void backend_worker(zsock_t *pipe, void *arg) {
  zpoller_t *poller = zpoller_new(pipe, NULL);
  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL,        1L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT,        40L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  int worker_id = (int) (intptr_t) arg;
  LDEBUG("backend: worker %d instantiated", worker_id);
  zsock_signal(pipe, 0);
  if (LGETLEVEL() <= LOG_LEVEL_INSEC)
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
  else
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

  void *in = NULL;
  while ((in = zpoller_wait(poller, -1))) {
    zmsg_t *msg = zmsg_recv(pipe);
    assert(msg);
    assert(zmsg_size(msg) > 0);
    char *request_id = zmsg_popstr(msg);
    if (!strcmp(request_id, "$TERM")) {
      LDEBUG("backend: worker %d: received shutdown signal", worker_id);
      free(request_id);
      zmsg_destroy(&msg);
      break;
    } else {
      perform_request(worker_id, request_id, curl, msg, pipe);
    }
    free(request_id);
    zmsg_destroy(&msg);
  }

  LDEBUG("backend: worker %d shutting down", worker_id);
  curl_easy_cleanup(curl);
}

void backend_service(zsock_t *pipe, void *arg) {
  zsock_t *incoming_requests = zsock_new_rep("inproc://backend");
  zpoller_t *poller = zpoller_new(pipe, incoming_requests, NULL);
  zsock_t *bcast = zsock_new_pub(">" EVENTS_PUB_ENDPOINT);
  long req_id = 0;

  typedef struct { zactor_t *actor; bool busy; } worker_t;
  worker_t *worker_pool = NULL;
  int worker_pool_size = 0;

  LINFO("backend: service initialized");
  zsock_signal(pipe, 0);

  while (1) {
    void *in = zpoller_wait(poller, -1);

    if (in == pipe) {
      LDEBUG("backend: received shutdown signal");
      break;
    } else if (in == NULL) {
      LWARN("backend: service interrupted!");
      break;
    } else if (in == incoming_requests) {
      zmsg_t *msg = zmsg_recv(incoming_requests);

      // generate an id and let the other thread go back to doing useful stuff
      // and then add the request id to the msg before the msg is forwarded to
      // a worker
      char *request_id = calloc(1, 128 * sizeof(char));
      sprintf(request_id, "request:%ld", ++req_id);
      zsock_send(incoming_requests, "ss", "broadcast_id", request_id);
      zmsg_pushstr(msg, request_id);
      free(request_id);

      // check out or instantiate a new background processor
      zactor_t *worker = NULL;
      int i;
      for (i = 0; i < worker_pool_size; i++) {
        // waiting worker found, forward the request along
        if (!worker_pool[i].busy) {
          worker = worker_pool[i].actor;
          worker_pool[i].busy = true;
          break;
        }
      }

      // if worker == NULL, we need to instantiate a new worker
      if (worker == NULL) {
        worker_pool = realloc(worker_pool, (++worker_pool_size) * sizeof(worker_t));
        worker = zactor_new(backend_worker, (void *) (intptr_t) worker_pool_size);
        worker_pool[worker_pool_size-1].actor = worker;
        worker_pool[worker_pool_size-1].busy = true;
        zpoller_add(poller, worker);
      }

      zmsg_send(&msg, worker);
    } else {
      LTRACE("backend: receiving response data");
      // in == a worker (outstanding request) that has completed and is ready
      // to be cleaned up
      zactor_t *worker = (zactor_t *) in;
      // receive the response msg from the worker, and broadcast it
      zmsg_t *msg = zmsg_recv(worker);
      zmsg_send(&msg, bcast);
      LTRACE("backend: response data broadcasted");
      // place the worker back into the pool
      int i;
      for (i = 0; i < worker_pool_size; i++)
        if (worker_pool[i].actor == worker) {
          worker_pool[i].busy = false;
          worker = NULL;
          break;
        }
      assert(worker == NULL);
    }
  }

  LINFO("backend: shutting down");
  int i;
  for (i = 0; i < worker_pool_size; i++) {
    zpoller_remove(poller, worker_pool[i].actor);
    zactor_destroy(&(worker_pool[i].actor));
  }
  free(worker_pool);
  zpoller_destroy(&poller);
  zsock_destroy(&incoming_requests);
  zsock_destroy(&bcast);
}

void add_whitelist_entry(const char *hostname) {
  whitelist_entry_t *entry;

  if (strlen(hostname) == 0) return;
  entry = (whitelist_entry_t *) calloc(1, sizeof(whitelist_entry_t));
  entry->hostname = strdup(hostname);

  HASH_ADD_KEYPTR(hh, whitelist, entry->hostname, strlen(entry->hostname), entry);
}

int init_whitelist(void) {
  char *whitelist_path = find_readable_file(NULL, "whitelist.txt");
  FILE *in;

  in = fopen(whitelist_path, "r");

  if (!in) {
    LERROR("backend: fatal: could not open whitelist file: %s", whitelist_path);
    free(whitelist_path);
    return 1;
  }
  free(whitelist_path);

  int ofs = 0;
  char line[256];
  memset(line, 0, sizeof(line));
  while (!feof(in)) {
    char ch = fgetc(in);
    if (ch == -1) break;
    if (ch == '\n') {
      add_whitelist_entry(line);
      memset(line, 0, sizeof(line));
      ofs = 0;
    } else {
      line[ofs++] = ch;
      line[ofs] = '\0';
      // check for overflow
      if (ofs == 255) {
        LWARN("backend: ignoring partial too-long whitelist entry: %s", line);
        ofs = 0;
        line[ofs] = '\0';
      }
    }
  }
  if (ofs > 0) add_whitelist_entry(line);

  fclose(in);
  return 0;
}

int init_backend_service(void) {
  if (init_whitelist()) return 1;

  service = zactor_new(backend_service, NULL);
  if (!service) {
    LERROR("backend: fatal: could not initialize backend service!");
    return 1;
  }

  return 0;
}

bool is_backend_service_running(void) { return !!service; }

void shutdown_backend_service(void) {
  whitelist_entry_t *entry = NULL, *tmp = NULL;
  HASH_ITER(hh, whitelist, entry, tmp) {
    HASH_DEL(whitelist, entry);
    free(entry->hostname);
    free(entry);
  }

  if (service) {
    zactor_destroy(&service);
    service = NULL;
  }
}
