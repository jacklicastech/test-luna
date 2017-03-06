#include <stdio.h>
#include <czmq.h>
#include <unistd.h>
#include <memory.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <resolv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "io/signals.h"
#include "rest_api.h"
#include "util/params_parser.h"
#include "util/headers_parser.h"
#include "services.h"
#include "util/api_request.h"
#include "util/string_helpers.h"

#define FAIL                 -1
#define ABORT_REQUEST        -4

/*
 * Given a glob of input, scans it for a request string in the form of
 *     VERB /path[ VERSION]\n
 * and then populates the given verb and path strings respectively. The HTTP
 * version is ignored at this time. If \r is present, it is ignored.
 * 
 * If this pattern can't be found, 0 is returned. Otherwise, the number of bytes
 * to the HTTP headers is returned.
 * 
 * Note: the verb may be up to 6 characters in length and will be NULL
 * terminated.
 */
unsigned int scan_http_path(const char *request,
                            unsigned int request_length,
                            char *verb,
                            char *path,
                            unsigned int max_path_len) {
#define STATE_HAVE_NOTHING 0
#define STATE_FOUND_START  1
#define STATE_HAVE_VERB    2
#define STATE_HAVE_PATH    3
    unsigned int i;
    unsigned int state = STATE_HAVE_NOTHING;
    unsigned int offset = 0;
    char ch;
    
    for (i = 0; i < request_length; i++) {
        ch = *(request + i);
        
        switch(ch) {
            case '\r': break;
            case '\n':
                if (state == STATE_HAVE_VERB) {
                    // HTTP version was omitted, bad omen, but we can recover
                    state = STATE_HAVE_PATH;
                }
                if (state == STATE_HAVE_PATH) {
                    // found end-of-line, time to stop
                    return i + 1;
                }
                break;
            case ' ':
                switch(state) {
                    case STATE_FOUND_START:
                        // space delimits end-of-verb, start-of-path
                        state = STATE_HAVE_VERB;
                        offset = 0;
                        break;
                    case STATE_HAVE_VERB:
                        // space delimits end-of-path, start-of-http-version
                        state = STATE_HAVE_PATH;
                }
                break;
            default:
                // any other character means we've found the beginning of the
                // verb, or we're already processing it
                if (state == STATE_HAVE_NOTHING)
                    state = STATE_FOUND_START;
                switch(state) {
                    case STATE_FOUND_START:
                        if (offset < MAX_HTTP_VERB_LENGTH - 1) {
                            *(verb+offset++) = ch;
                            *(verb+offset) = '\0';
                        }
                        break;
                    case STATE_HAVE_VERB:
                        if (offset < max_path_len - 1) {
                            *(path+offset++) = ch;
                            *(path+offset) = '\0';
                        }
                }
        }
    }
    return request_length;
#undef STATE_HAVE_NOTHING
#undef STATE_FOUND_START
#undef STATE_HAVE_VERB
#undef STATE_HAVE_PATH
}

static void dump_certs(SSL* ssl) {
    X509 *cert;
    char *line;

    cert = SSL_get_peer_certificate(ssl);	/* Get certificates (if available) */
    if (cert != NULL) {
        LDEBUG("https-request: Client offered certificates:");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        LDEBUG("https-request: Subject: %s", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        LDEBUG("https-request: Issuer: %s", line);
        free(line);
        X509_free(cert);
    } else {
        LDEBUG("https-request: No client certificates offered.");
    }
}

static bool socket_ready(int fd, double timeout) {
   int status;
   fd_set fds;
   struct timeval tv;
   FD_ZERO(&fds);
   FD_SET(fd, &fds);
   tv.tv_sec  = (long)timeout; // cast needed for C++
   tv.tv_usec = (long)((timeout - tv.tv_sec) * 1000000); // 'suseconds_t'

   while (1) {
      if (!(status = select(fd + 1, &fds, 0, 0, &tv)))
         return false;
      else if (status > 0 && FD_ISSET(fd, &fds))
         return true;
      else if (status > 0)
         LERROR("https-request: unexpected select status");
      else
         LERROR("https-request: error: %s during socket select", strerror(errno));
   }
}

static double time_secs_since_epoch()
{
    struct timeval tm;
    gettimeofday( &tm, NULL );
    return (double)tm.tv_sec + (double)tm.tv_usec / 1000000.0;
}

static bool flush_then_close(int fd, double timeout) {
   const double start = time_secs_since_epoch();
   char discard[99];
   assert(SHUT_WR == 1);
   if (shutdown(fd, 1) != -1)
      while (time_secs_since_epoch() < start + timeout)
         while (socket_ready(fd, 0.01)) // can block for 0.01 secs
            if (!read(fd, discard, sizeof discard))
               return true; // success!
   return false;
}

/*
 * Dispatches a single request by connecting to the internal socket and
 * sending the request data to the socket. The reason the socket is used is
 * to maximize the freedom of the actual API implementation. At time of
 * writing, the API implementation is likely to be running in Lua on the main
 * thread. Not only is a zeromq socket a convenient way to share data with
 * the API, it also allows the API to evolve in virtually any way as long as
 * it communicates via ZMQ.
 */
char *dispatch_request(const char *verb, const char *path,
                       header_t **headers, const char *request_body) {
    char *headers_str = (char *) calloc(4, sizeof(char));
    zsock_t *sock = zsock_new_req("inproc://api");
    header_t *header, *tmp;
    zmsg_t *msg = zmsg_new();
    zpoller_t *poller = NULL;
    char *response_status = NULL,
         *response_headers = NULL,
         *response_body = NULL,
         *response = NULL;
         // *strtmp = NULL;
    void *in = NULL;

    assert(sock);
    poller = zpoller_new(sock, NULL);

    HASH_ITER(hh, *headers, header, tmp) {
        int len1 = strlen(headers_str);
        int len2 = strlen(header->name);
        int len3 = strlen(header->value);
        char *str = (char *) calloc(len1 + len2 + len3 + 8, sizeof(char));
        sprintf(str, "%s\n%s: %s", headers_str, header->name, header->value);
        free(headers_str);
        headers_str = str;
    }

    zmsg_addstr(msg, verb);
    zmsg_addstr(msg, "path");
    zmsg_addstr(msg, path);
    zmsg_addstr(msg, "headers");
    zmsg_addstr(msg, headers_str);
    zmsg_addstr(msg, "body");
    zmsg_addstr(msg, request_body == NULL ? "" : request_body);
    zmsg_send(&msg, sock);

    in = zpoller_wait(poller, 5000); // 5 second timeout
    if (in == NULL) {
        LERROR("https-request: timed out or interrupted while waiting for API response");
        msg = zmsg_new();
        zmsg_addstr(msg, "503 Gateway Unavailable");
        zmsg_addstr(msg, "Content-type: application/json");
        zmsg_addstr(msg, "{\"error\":\"gateway unavailable\"}");
    } else {
        msg = zmsg_recv(sock);
    }
        
    if (zmsg_size(msg) != 3) {
        LERROR("https-request: expected response to have exactly 3 frames but it had %ld", zmsg_size(msg));
        response_status = strdup("500 Internal Server Error");
        response_headers = strdup("Content-type: application/json");
        response_body = strdup("{\"error\":\"internal server error\"}");
    } else {
        response_status = zmsg_popstr(msg);
        response_headers = zmsg_popstr(msg);
        response_body = zmsg_popstr(msg);
    }

    (void) trim(response_headers);
    response = (char *) calloc(strlen(response_status) +
                               strlen(response_headers) +
                               strlen(response_body) + 1024, sizeof(char));
    if (strlen(response_headers) > 0) {
        sprintf(response, "HTTP/1.1 %s\r\n"
                          "%s\r\n"
                          "Content-length: %ld\r\n"
                          "\r\n"
                          "%s", response_status, response_headers, (long) strlen(response_body), response_body);
    } else {
        sprintf(response, "HTTP/1.1 %s\r\n"
                          "Content-length: %ld\r\n"
                          "\r\n"
                          "%s", response_status, (long) strlen(response_body), response_body);
    }
    // LDEBUG("https-request: Writing response:\n  %s", response);
    
    free(response_status);
    free(response_headers);
    free(response_body);
    free(headers_str);
    zpoller_destroy(&poller);
    zmsg_destroy(&msg);
    zsock_destroy(&sock);
    
    return response;
}

/*
 * Processes a single request from the HTTPS server. We really only want to
 * allow 1 request to process at once, but we still need to do it in the
 * background in order to allow HTTPS to receive new connections, so that we
 * can emit a "still busy" type of error if connections are received while the
 * txn is still in progress. Otherwise the server would just not respond and
 * eventually time out, instead of producing a coherent error. Furthermore, it
 * makes sense to architect this way in case new APIs are added that can in fact
 * be processed while a transaction is in progress -- such as when querying
 * for usage statistics.
 */
void https_api_handle_request(zsock_t *pipe, void *args) {
    SSL *ssl = (SSL *) args;
    char buf[2048];
    int sd, bytes;
    char verb[MAX_HTTP_VERB_LENGTH];
    char path[MAX_HTTP_PATH_LENGTH];
    char *request_headers_and_body, *request_body = NULL;
    unsigned int offset;
    int err;
    int i;
    //int response_status;
//    api_endpoint endpoint;
    request_t *request;
    header_t *content_length = NULL;
    
    zsock_signal(pipe, SIGNAL_ACTOR_INITIALIZED);

    STACK_OF(SSL_CIPHER) *sk = SSL_get_ciphers(ssl);
    LDEBUG("webserver: ssl: available ciphers:");
    for (i = 0; sk && i < sk_SSL_CIPHER_num(sk); i++) {
        LDEBUG("webserver: ssl:   %s", sk_SSL_CIPHER_value(sk, i)->name);
    }

    if (SSL_accept(ssl) == FAIL) {                  /* do SSL-protocol accept */
        LERROR("https-request: SSL accept failed: %s", ERR_error_string(ERR_get_error(), buf));
    } else {
        dump_certs(ssl);                            /* get any certificates */
        bytes = SSL_read(ssl, buf, sizeof(buf));    /* get request */
        if (bytes > 0) {
            LDEBUG("https-request: read %d bytes", bytes);
            buf[bytes] = '\0';
            // parse full request into HTTP verb, URI and body
            offset = scan_http_path(buf, (unsigned) bytes, verb, path, MAX_HTTP_PATH_LENGTH);
            request_headers_and_body = buf + offset;
            LINFO("https-request: Processing request: %s %s", verb, path);
            request = (request_t *) calloc(1, sizeof(request_t));
            assert(strlen(request_headers_and_body) == bytes - offset);
            offset += parse_headers(&(request->request_headers), request_headers_and_body, bytes - offset);
            if (offset < strlen(buf)) {
                request_body = (char *) calloc(bytes - offset + 4, sizeof(char));
                memcpy(request_body, buf + offset, bytes - offset + 1);
                request_body[bytes-offset+1] = '\0';
                assert(strlen(request_body) == bytes - offset);
            } else {
                LDEBUG("https-request: no request parameters received");
                err = 0;
                HASH_FIND_STR(request->request_headers, "content-length", content_length);
                if (content_length) {
                    int length = atoi(content_length->value);
                    char *content = (char *) calloc(length + 1, sizeof(char));
                    char *ptr = content;
                    int remaining = length;
                    LDEBUG("https-request: client said we should receive %d bytes, waiting for it", length);
                    while (remaining > 0) {
                        // FIXME a select here would be more efficient
                        bytes = SSL_read(ssl, ptr, remaining * sizeof(char));
                        if (bytes <= 0) {
                            // FIXME the error may not be fatal and in that case a retry should be attempted
                            LWARN("https-request: abort due to error: %d", SSL_get_error(ssl, bytes));
                            err = ABORT_REQUEST;
                            break;
                        } else {
                            ptr += bytes;
                            remaining -= bytes;
                        }
                    };
                    if (err != ABORT_REQUEST) {
                        LDEBUG("https-request: request body: %s", content);
                        // err = parse_request_params_json(&(request->params), content, length);
                        request_body = content;
                    } else {
                        free(content);
                    }
                } else {
                    LDEBUG("https-request: no parameter data received");
                }
            }

            char *response = dispatch_request(verb, path, &(request->request_headers), request_body);
            SSL_write(ssl, response, strlen(response));
            LDEBUG("https-request: response has been sent");
            free(response);

            if (request_body) free(request_body);
            LDEBUG("https-request: freeing request headers");
            free_headers(&(request->request_headers));
        } else {
            LERROR("https-request: SSL read failed: %s", ERR_error_string(ERR_get_error(), buf));
        }
    }

    LDEBUG("https-request: ending SSL session.");
    sd = SSL_get_fd(ssl);                           /* get socket connection */
    SSL_shutdown(ssl); // see https://john.nachtimwald.com/2014/10/05/server-side-session-cache-in-openssl/
    SSL_free(ssl);                                  /* release SSL state */
    if (sd == -1) {
        LERROR("https-request: could not get file descriptor: could not close connection");
    } else {
        flush_then_close(sd, 1.0);
    }
    LINFO("https-request: request complete.");
    zsock_signal(pipe, SIGNAL_REQUEST_COMPLETE);
    zsock_signal(pipe, SIGNAL_NO_ERROR);
}
