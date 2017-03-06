#define LUA_LIB
#include "config.h"
#include <stdio.h>
#include "services.h"
#include "lua.h"
#include "lauxlib.h"
#if HAVE_CTOS
#include <ctosapi.h>
#endif

int check_rs232_err(lua_State *L, int err) {
  #if HAVE_CTOS
    switch(err) {
      case d_RS232_INVALID_PARA:
        lua_pushstring(L, "invalid parameter");
        return 1;
      case d_RS232_HAL_FAULT:
        lua_pushstring(L, "hardware fault");
        return 1;
      case d_RS232_BUSY:
        lua_pushstring(L, "busy");
        return 1;
      case d_RS232_NOT_OPEN:
        lua_pushstring(L, "port is not open");
        return 1;
      case d_RS232_NOT_SUPPORT:
        lua_pushstring(L, "operation not supported");
        return 1;
    }
  #endif
  return 0;
}

int decode_port(lua_State *L, const char *port) {
  #if HAVE_CTOS
    if (!strcmp(port, "com1") || !strcmp(port, "COM1") || !strcmp(port, "1")) {
      return d_COM1;
    } else if (!strcmp(port, "com2") || !strcmp(port, "COM2") || !strcmp(port, "2")) {
      return d_COM2;
    } else if (!strcmp(port, "com3") || !strcmp(port, "COM3") || !strcmp(port, "3")) {
      return d_COM3;
    }
  #endif
  lua_pushfstring(L, "invalid COM port specified: %s (expected COM1, COM2, etc)", port);
  return -1;
}

/*
 * Close a COM port. Specify the COM port as the only argument ('COM1',
 * 'COM2', etc).
 *
 * If an error occurs, a string will be returned. Otherwise the return value
 * is `nil`.
 * 
 * Examples:
 * 
 *     rs232 = require('rs232')
 *     err = rs232.close('COM1')
 *     if err then print(err) end
 *
 */
int rs232_close(lua_State *L) {
  int port = decode_port(L, lua_tostring(L, 1));
  if (port == -1) return 1;

  int err = 0;
  #if HAVE_CTOS
    err = CTOS_RS232Close((BYTE) port);
  #endif
  return check_rs232_err(L, err);
}

/*
 * Send some data over a COM port. Specify the COM port as the first argument
 * ('COM1', 'COM2', etc). Specify the data to send as the second argument.
 * Optionally, specify a timeout in milliseconds as the third argument.
 *
 * This function will block until all data has been sent, or an unrecoverable
 * error has occurred, or a timeout has occurred.
 *
 * If an error occurs, it will be returned as a string. If a timeout occurs,
 * the string 'timeout' will be returned. Otherwise the return value is `nil`.
 *
 * Examples:
 *
 *     rs232 = require('rs232')
 *     err = rs232.open(...) -- see `open`
 *     if err then print(err)
 *     else
 *       err = rs232.send('COM1', 'this is some data')
 *       err = rs232.send('COM1', 'this is some data', 100)
 *     end
 * 
 */
int rs232_send(lua_State *L) {
  int port = decode_port(L, lua_tostring(L, 1));
  if (port == -1) return 1;

  size_t data_len;
  const char *data = lua_tolstring(L, 1, &data_len);

  long timeout_ns = -1;
  if (!lua_isnil(L, 2)) timeout_ns = (long) lua_tonumber(L, 2) * 1000000;
  struct timespec start_time;
  struct timespec current_time;
  if (clock_gettime(CLOCK_MONOTONIC, &start_time)) {
    lua_pushstring(L, "could not initialize clock");
    return 1;
  }

  #if HAVE_CTOS
    int err = CTOS_RS232GetCTS(port);
    if (err != d_OK) return check_rs232_err(L, err);

    while (CTOS_RS232TxReady(port) != d_OK) {
      if (timeout_ns == -1) continue;

      if (clock_gettime(CLOCK_MONOTONIC, &current_time)) {
        lua_pushstring(L, "could not check for timeout");
        return 1;
      }

      if ((current_time.tv_sec * 1e+9 + current_time.tv_nsec) -
          (start_time.tv_sec * 1e+9   + start_time.tv_nsec) > timeout_ns) {
        lua_pushstring(L, "timeout");
        return 1;
      }
    }

    err = CTOS_RS232TxData(port, (BYTE *) data, data_len);
    return check_rs232_err(L, err);
  #else
    (void) data;
    (void) current_time;
    (void) timeout_ns;
    lua_pushstring(L, "not implemented");
    return 1;
  #endif
}

/*
 * Receive some data over a COM port. Specify the COM port as the first
 * argument ('COM1', 'COM2', etc). Optionally, specify a timeout in
 * milliseconds as the second argument.
 *
 * This function will block until data has been received, or an unrecoverable
 * error has occurred, or a timeout has occurred.
 *
 * There are two return values for this function. The first will be `nil` on
 * success, or the string value 'timeout', or a string containing an error
 * message. The second is the string of data that was received, or `nil`.
 *
 * Examples:
 *
 *     rs232 = require('rs232')
 *     err = rs232.open(...) -- see `open`
 *     if err then print(err)
 *     else
 *       err, data = rs232.recv('COM1')
 *       err, data = rs232.recv('COM1', 100)
 *     end
 * 
 */
int rs232_recv(lua_State *L) {
  int port = decode_port(L, lua_tostring(L, 1));
  if (port == -1) return 1;

  long timeout_ns = -1;
  if (!lua_isnil(L, 2)) timeout_ns = (long) lua_tonumber(L, 2) * 1000000;
  struct timespec start_time;
  struct timespec current_time;
  if (clock_gettime(CLOCK_MONOTONIC, &start_time)) {
    lua_pushstring(L, "could not initialize clock");
    return 1;
  }

  #if HAVE_CTOS
    USHORT len = 0;
    BYTE buf[65536];
    while (len == 0) {
      int err = CTOS_RS232RxReady(port, &len);
      if (err != d_OK) return check_rs232_err(L, err);

      if (len == 0) {
        if (timeout_ns == -1) continue;

        if (clock_gettime(CLOCK_MONOTONIC, &current_time)) {
          lua_pushstring(L, "could not check for timeout");
          return 1;
        }

        if ((current_time.tv_sec * 1e+9 + current_time.tv_nsec) -
            (start_time.tv_sec   * 1e+9 + start_time.tv_nsec) > timeout_ns) {
          lua_pushstring(L, "timeout");
          return 1;
        }

        continue;
      }

      err = CTOS_RS232RxData(d_COM1, buf, &len);
      if (err != d_OK) return check_rs232_err(L, err);
    } 

    lua_pushnil(L);
    lua_pushlstring(L, (char *) buf, len);
    return 2;
  #else
    (void) current_time;
    (void) timeout_ns;
    lua_pushstring(L, "not implemented");
    return 1;
  #endif
}

/*
 * Flush the transmit (send) or receive (recv) buffer, or both. Specify the
 * COM port as the first argument (e.g. 'COM1'). Specify 'recv' or 'send' as
 * the second argument, or `nil` to flush both buffers.
 *
 * If an error occurs, a string will be returned. Otherwise the return value
 * is `nil`.
 * 
 * Examples:
 * 
 *     rs232 = require('rs232')
 *     rs232.flush('COM1')
 *     rs232.flush('COM1', 'send')
 *     rs232.flush('COM1', 'recv')
 *
 */
int rs232_flush(lua_State *L) {
  int port = decode_port(L, lua_tostring(L, 1));
  if (port == -1) return 1;

  int err = 0;
  if (lua_isnil(L, 2)) {
    #if HAVE_CTOS
      err = CTOS_RS232FlushRxBuffer(port);
      if (err != d_OK) return check_rs232_err(L, err);
      err = CTOS_RS232FlushTxBuffer(port);
    #endif
  } else if (!strcmp(lua_tostring(L, 2), "recv")) {
    #if HAVE_CTOS
      err = CTOS_RS232FlushRxBuffer(port);
    #endif
  } else if (!strcmp(lua_tostring(L, 2), "send")) {
    #if HAVE_CTOS
      err = CTOS_RS232FlushTxBuffer(port);
    #endif
  } else {
    lua_pushfstring(L, "invalid buffer specification: %s", lua_tostring(L, 2));
    return 1;
  }

  return check_rs232_err(L, err);
}

/*
 * Open a COM port. Specify the COM port as the first argument ('COM1',
 * 'COM2', etc). Specify baud rate as the second argument. Specify parity
 * as the third argument ('even', 'odd', or 'none'). Specify the number of
 * data bits as the fourth argument (7 or 8). Specify the number of stop bits
 * as the fifth argument (1 or 2).
 *
 * If an error occurs, a string will be returned. Otherwise the return value
 * is `nil`.
 * 
 * Examples:
 * 
 *     rs232 = require('rs232')
 *     err = rs232.open('COM1', 9600, 'none', 8, 1)
 *     if err then print(err) end
 *
 */
int rs232_open(lua_State *L) {
  int port = decode_port(L, lua_tostring(L, 1));
  int baud = atoi(lua_tostring(L, 2));
  const char *str_parity = lua_tostring(L, 3);
  int data_bits = atoi(lua_tostring(L, 4));
  int stop_bits = atoi(lua_tostring(L, 5));
  int parity;

  if (port == -1) return 1;

  switch(baud) {
    case 115200:
    case 57600:
    case 38400:
    case 19200:
    case 9600:
      break;
    default:
      lua_pushfstring(L, "invalid baud rate: %d (valid values: 115200, 57600, 38400, 19200, 9600)", baud);
      return 1;
  }

  if (!strcmp(str_parity, "even")) parity = 'E';
  else if (!strcmp(str_parity, "odd")) parity = 'O';
  else if (!strcmp(str_parity, "none")) parity = 'N';
  else {
    lua_pushfstring(L, "invalid parity: %s (expected 'even', 'odd', 'none')", str_parity);
    return 1;
  }

  if (data_bits != 7 && data_bits != 8) {
    lua_pushfstring(L, "invalid data bits: %d (expected 7 or 8)", data_bits);
    return 1;
  }

  if (stop_bits != 1 && stop_bits != 2) {
    lua_pushfstring(L, "invalid stop bits: %d (expected 1 or 2)", stop_bits);
    return 1;
  }


  int err = 0;
  #if HAVE_CTOS
    err = CTOS_RS232Open((BYTE) port, (ULONG) baud, (BYTE) parity,
                         (BYTE) data_bits, (BYTE) stop_bits);
    if (err != d_OK) return check_rs232_err(L, err);
    // signal ready to send
    err = CTOS_RS232SetRTS((BYTE) port, d_ON);
  #else
    (void) parity;
  #endif
  return check_rs232_err(L, err);
}

static const luaL_Reg rs232_methods[] = {
  {"open", rs232_open},
  {"send", rs232_send},
  {"recv", rs232_recv},
  {"close",rs232_close},
  {"flush",rs232_flush},
  {NULL,  NULL}
};

LUALIB_API int luaopen_rs232(lua_State *L) {
  lua_newtable(L);
  luaL_setfuncs(L, rs232_methods, 0);
  return 1;
}

void shutdown_rs232_lua(lua_State *L) {
  (void) L;
}
