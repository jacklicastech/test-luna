#define LUA_LIB
#include "config.h"
#include <stdio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "services.h"
#include "lua.h"
#include "lauxlib.h"
#include "util/detokenize_template.h"
#include "util/files.h"
#if HAVE_CTOS
#include <ctosapi.h>
#endif

extern char cacerts_bundle[PATH_MAX];

typedef struct {
  char *human;
  int sockfd;
  SSL *ssl;
  char *pem_password;
  bool is_secure;
} lua_socket_t;

// Whitelist stuff copied from backend code. TODO remove duplication.
//////////////////////////////////////////////////////////////////////////////
typedef struct {
  char *hostname;
  UT_hash_handle hh;
} whitelist_entry_t;

static whitelist_entry_t *whitelist = NULL;

int is_whitelisted(const char *hostname, const size_t len) {
  whitelist_entry_t *entry = NULL;
  HASH_FIND(hh, whitelist, hostname, len, entry);
  if (entry == NULL) return 0;
  return 1;
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
//////////////////////////////////////////////////////////////////////////////

int pem_passwd_cb(char *buf, int size, int rwflag, void *password) {
  (void) rwflag;
  strncpy(buf, password, size);
  buf[size - 1] = '\0';
  return strlen(buf);
}

/*
 * Closes the connection. Note that you don't have to do this explicitly,
 * because when the socket is garbage collected by Lua it will be closed.
 * However, it may be useful to be sure that a connection is closed at a
 * specific point in time, so this method is provided.
 *
 * Examples:
 * 
 *     socket = require('socket')
 *     sock = socket.tcp('192.168.0.1', 8090)
 *     err = sock:send("data")
 *     sock:close()
 *
 */
int socket_destroy(lua_State *L) {
  lua_socket_t *lsock = (lua_socket_t *) luaL_checkudata(L, 1, "LSocket");
  if (lsock->pem_password) {
    free(lsock->pem_password);
    lsock->pem_password = NULL;
  }
  if (lsock->ssl) {
    SSL_shutdown(lsock->ssl);
    SSL_free(lsock->ssl);
    lsock->ssl = NULL;
  }
  if (lsock->sockfd >= 0) {
    close(lsock->sockfd);
    lsock->sockfd = -1;
  }
  return 0;
}

int socket_tostring(lua_State *L) {
  lua_socket_t *lsock = (lua_socket_t *) luaL_checkudata(L, 1, "LSocket");
  lua_pushstring(L, lsock->human);
  return 1;
}

/*
 * Sends data over the active TCP or TLS connection. If data can't immediately
 * be sent, this function will block until all of the data has been sent.
 *
 * If some unrecoverable error occurs, it will be returned as a string.
 * Otherwise `nil` is returned.
 *
 * Examples:
 * 
 *     socket = require('socket')
 *     sock = socket.tcp('192.168.0.1', 8090)
 *     err = sock:send("data")
 *
 *     sock = socket.tls('192.168.0.1', 8090)
 *     err = sock:send("data")
 *
 */
int socket_send(lua_State *L) {
  lua_socket_t *lsock = (lua_socket_t *) luaL_checkudata(L, 1, "LSocket");
  size_t len;
  const char *buffer = lua_tolstring(L, 2, &len);
  char *detokenized = NULL;
  if (lsock->is_secure) {
    detokenized = detokenize_template(buffer, &len);
    buffer = detokenized;
    if (!detokenized) {
      lua_pushstring(L, "couldn't detokenize template");
      return 1;
    }
  }

  while (len > 0) {
    int n;
    if (lsock->ssl) {
      n = SSL_write(lsock->ssl, buffer, len);
      if (n <= 0) {
        if (detokenized) free(detokenized);
        char errbuf[128];
        memset(errbuf, 0, sizeof(errbuf));
        lua_pushstring(L, ERR_error_string(SSL_get_error(lsock->ssl, n), errbuf));
        return 1;
      } else {
        len -= n;
        buffer += n;
      }
    } else {
      n = write(lsock->sockfd, buffer, len);
      if (n < 0) {
        switch(errno) {
          case EAGAIN:
            break;
          default:
            if (detokenized) free(detokenized);
            lua_pushstring(L, strerror(errno));
            return 1;
        }
      } else {
        len -= n;
        buffer += n;
      }
    }
  }

  if (detokenized) free(detokenized);
  return 0;
}

/*
 * Receives data over the active TCP or TLS connection. Blocks until data is
 * available if data is not initially available.
 *
 * If some unrecoverable error occurs while reading, it will be returned as
 * the second argument and the first argument will be `nil`.
 *
 * NOTE: Although this method will block until data is ready, it does not
 * make any guarantees about how much data will be returned. If only a portion
 * of an underlying message is available, then that portion will be returned
 * without waiting for the remainder of the message. This is because the
 * `recv` method does not know anything about the underlying messages that it
 * is receiving. For example, it does not know if it's dealing with JSON, XML
 * or line-buffered data. Up to 4096 bytes of data will be returned in a
 * single call, and you should always perform your own checks before assuming
 * that the data you received constitutes a complete message.
 *
 * Examples:
 * 
 *     -- Holds unprocessed messages. Data will be accumulated here and then
 *     -- removed from here as message delimiters are discovered.
 *     buffered_messages = ''
 *     
 *     -- Reads 1 message, assuming each new line represents the end of a
 *     -- message. If a portion of another message is received, buffer it
 *     -- so that the rest can be received later. If a socket error occurs,
 *     -- `nil` is returned and the error is returned as a second argument.
 *     function recv_message(sock)
 *       while true do
 *         data, err = sock:recv()
 *         if err then
 *           return nil, err
 *         else
 *           buffered_messages = buffered_messages .. data
 *         end
 *         delimiter = data:find("\n")
 *         if delimeter > 0 then
 *           message = data:sub(1, delimiter)
 *           buffered_messages = data:sub(index + 1, #buffered_messages)
 *           return message
 *         end
 *       end
 *     end
 *
 *     socket = require('socket')
 *     sock = socket.tcp('www.google.com', 443)
 *     message, err = recv_message(sock)
 *
 */
int socket_recv(lua_State *L) {
  while (true) {
    lua_socket_t *lsock = (lua_socket_t *) luaL_checkudata(L, 1, "LSocket");
    char buf[4096];
    ssize_t len;
    if (lsock->ssl) {
      len = SSL_read(lsock->ssl, buf, 4096);
      if (len <= 0) {
        lua_pushnil(L);
        char errbuf[128];
        memset(errbuf, 0, sizeof(errbuf));
        lua_pushstring(L, ERR_error_string(SSL_get_error(lsock->ssl, len), errbuf));
        return 2;
      } else {
        lua_pushlstring(L, buf, len);
        return 1;
      }
    } else {
      len = read(lsock->sockfd, buf, 4096);
      if (len < 0) {
        switch(errno) {
          case EAGAIN:
            break;
          default:
            lua_pushnil(L);
            lua_pushstring(L, strerror(errno));
            return 2;
        }
      } else {
        lua_pushlstring(L, buf, len);
        return 1;
      }
    }
  }
}

/*
 * Returns `true` if the socket is a TLS connection *and* the host to which it
 * is connected is present in the whitelist.
 *
 *     socket = require('socket')
 *     sock = socket.tls('www.google.com', 443)
 *     print(sock:is_secure())
 *
 */
int socket_is_secure(lua_State *L) {
  lua_socket_t *lsock = (lua_socket_t *) luaL_checkudata(L, 1, "LSocket");
  lua_pushboolean(L, lsock->is_secure);
  return 1;
}

lua_socket_t *lsocket_new_generic(lua_State *L) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));
    return NULL;
  }

  struct hostent *server = gethostbyname(lua_tostring(L, 1));
  if (server == NULL) {
    lua_pushnil(L);
    lua_pushstring(L, "host not found");
    close(sockfd);
    return NULL;
  }
  int portno = lua_tonumber(L, 2);

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(portno);
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) {
    lua_pushnil(L);
    lua_pushfstring(L, "connect failed: %s", strerror(errno));
    close(sockfd);
    return NULL;
  }

  lua_socket_t *lsock = (lua_socket_t *) lua_newuserdata(L, sizeof(lua_socket_t));
  lsock->human = malloc(strlen(lua_tostring(L, 1)) + strlen(lua_tostring(L, 2)) + 32);
  lsock->sockfd = sockfd;
  lsock->pem_password = NULL;
  lsock->ssl = NULL;
  lsock->is_secure = false;
  sprintf(lsock->human, "<tcp %s:%s>", lua_tostring(L, 1), lua_tostring(L, 2));

  // Add the metatable to the stack.
  luaL_getmetatable(L, "LSocket");
  // Set the metatable on the userdata.
  lua_setmetatable(L, -2);

  return lsock;
}

/*
 * Creates a new TCP socket connection to the specified host and port. Note:
 * Raw TCP sockets are not a secure form of communication, so sensitive data
 * will not be transmitted.
 *
 * Returns `nil` if the socket cannot be created. A second return value
 * contains an error message as a string.
 * 
 * Examples:
 * 
 *     socket = require('socket')
 *     sock, err = socket.tcp('192.168.0.1', 8090)
 *
 */
int socket_tcp(lua_State *L) {
  lua_socket_t *lsock = lsocket_new_generic(L);
  if (!lsock) return 2;
  return 1;
}

/*
 * Creates a new secure TCP socket using TLS to the specified host and port.
 * Because the data is encrypted, sensitive data may be transmitted over this
 * type of connection, as long as the host name appears in the whitelist file.
 *
 * If the host name does not appear in the whitelist, the connection will
 * still be encrypted using TLS, but sensitive data will not be transmitted.
 *
 * Optionally, the third argument can contain the path to a certificate PEM
 * file.
 *
 * Optionally, the fourth argument can contain the path to a private key PEM
 * file.
 *
 * Optionally, the fifth argument can contain a password for the private key.
 *
 * Returns `nil` if the socket cannot be created. A second return value
 * contains an error message as a string.
 * 
 * Examples:
 * 
 *     socket = require('socket')
 *     sock, err = socket.tls('192.168.0.1', 8090)
 *
 *     sock, err = socket.tls('192.168.0.1', 8090,
 *                            'path/to/cert.pem',
 *                            'path/to/privkey.pem')
 *
 *     sock, err = socket.tls('192.168.0.1', 8090,
 *                            'path/to/cert.pem',
 *                            'path/to/privkey.pem',
 *                            'privkey-decrypt-password')
 *
 */
int socket_tls(lua_State *L) {
  int top = lua_gettop(L);

  lua_socket_t *lsock = lsocket_new_generic(L);
  if (!lsock) return 2;

  // initialize ctx
  const SSL_METHOD *method = TLSv1_2_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (top > 2)
    SSL_CTX_use_certificate_file(ctx, lua_tostring(L, 3), SSL_FILETYPE_PEM);
  if (top > 3) {
    lsock->pem_password = strdup(lua_tostring(L, 4));
    SSL_CTX_set_default_passwd_cb(ctx, pem_passwd_cb);
    SSL_CTX_set_default_passwd_cb_userdata(ctx, lsock->pem_password);
  }
  if (top > 4)
    SSL_CTX_use_PrivateKey_file(ctx, lua_tostring(L, 5), SSL_FILETYPE_PEM);
  SSL_CTX_load_verify_locations(ctx, cacerts_bundle, 0);

  lsock->ssl = SSL_new(ctx);
  if (lsock->ssl == NULL) goto handle_err;

  BIO *sbio = BIO_new_socket(lsock->sockfd, BIO_NOCLOSE);
  if (sbio == NULL) goto handle_err;
  SSL_set_bio(lsock->ssl, sbio, sbio);

  int err = SSL_connect(lsock->ssl);
  if (err <= 0) goto handle_err;

  memcpy(lsock->human + 1, "tls", 3);
  if (is_whitelisted(lua_tostring(L, 1), strlen(lua_tostring(L, 1))))
    lsock->is_secure = true;
  return 1;

handle_err:
  lua_pop(L, -1);
  lua_pushnil(L);
  char errbuf[128];
  memset(errbuf, 0, sizeof(errbuf));
  lua_pushstring(L, ERR_error_string(ERR_get_error(), errbuf));
  return 2;
}

static const luaL_Reg lsocket_methods[] = {
  { "send",       socket_send     },
  { "recv",       socket_recv     },
  { "is_secure",  socket_is_secure},
  { "close",      socket_destroy  },
  { "__gc",       socket_destroy  },
  { "__tostring", socket_tostring },
  {NULL,  NULL}
};

static const luaL_Reg lsocket_functions[] = {
  {"tcp", socket_tcp},
  {"tls", socket_tls},
  {NULL,  NULL}
};

LUALIB_API int luaopen_socket(lua_State *L) {
  (void) init_whitelist();

  // Create the metatable and put it on the stack.
  luaL_newmetatable(L, "LSocket");
  // Duplicate the metatable on the stack (We now have 2).
  lua_pushvalue(L, -1);
  // Pop the first metatable off the stack and assign it to __index
  // of the second one. We set the metatable for the table to itself.
  // This is equivalent to the following in lua:
  // metatable = {}
  // metatable.__index = metatable
  lua_setfield(L, -2, "__index");

  // Set the methods to the metatable that should be accessed via object:func
  luaL_setfuncs(L, lsocket_methods, 0);

  // Register the object.func functions into the table that is at the top of
  // the stack.
  luaL_newlib(L, lsocket_functions);

  return 1;
}

void shutdown_socket_lua(lua_State *L) {
  (void) L;
  whitelist_entry_t *entry = NULL, *tmp = NULL;
  HASH_ITER(hh, whitelist, entry, tmp) {
    HASH_DEL(whitelist, entry);
    free(entry->hostname);
    free(entry);
  }
}
