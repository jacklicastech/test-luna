#define LUA_LIB

#include <inttypes.h>
#include <lua.h>
#include <lauxlib.h>

#include <czmq.h>
#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "services/logger.h"

#define MT_ZSOCK  "MT_ZSOCK"

typedef struct {
    zsock_t   *zsock;
    zpoller_t *poller;
    int        as_coroutine; // see socket_as_coroutine()
    char      *human;        // human-readable info
} lsock_t;

static int Lzmq_push_error(lua_State *L)
{
    // const char *error;
    LWARN("lua: zmq: failed: %s", zmq_strerror(zmq_errno()));
    return luaL_error(L, "lzmq: failed: %s", zmq_strerror(zmq_errno()));
    // lua_pushnil(L);
    // switch(zmq_errno()) {
    // case EAGAIN:
    //     lua_pushliteral(L, "timeout");
    //     break;
    // case ETERM:
    //     lua_pushliteral(L, "closed");
    //     break;
    // default:
    //     error = zmq_strerror(zmq_errno());
    //     lua_pushlstring(L, error, strlen(error));
    //     break;
    // }
    // return 2;
}

/*
 * Creates a new `SUB` (subscription) socket. The first argument is the
 * endpoint URI and is required. The second argument may be a
 * topic prefix to subscribe to. If omitted, all messages published
 * to this endpoint will be subscribed to.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.sub("inproc://events")
 *     sock = zmq.sub("inproc://events", "keyup")
 */
static int lzmq_sub(lua_State *L) {
    const char *endpoint = lua_tostring(L, 1),
               *channel = lua_tostring(L, 2);
    lsock_t *sock = (lsock_t *) lua_newuserdata(L, sizeof(lsock_t));
    if (channel == NULL) channel = "";
    luaL_getmetatable(L, MT_ZSOCK);
    lua_setmetatable(L, -2);
    sock->zsock = zsock_new_sub(endpoint, channel);
    sock->poller = zpoller_new(sock->zsock, NULL);
    sock->as_coroutine = 0;
    sock->human = (char *) calloc(strlen(endpoint) + strlen(channel) + 33, sizeof(char));
    sprintf(sock->human, "<zsock:0x%08" PRIxPTR " sub:%s/%s>", (uintptr_t) sock->zsock, endpoint, channel);
    LDEBUG("lua: zmq: creating socket: %s", sock->human);
    if (!sock->zsock) return Lzmq_push_error(L);
    return 1;
}

/*
 * Creates a new `PUB` (publication) socket. The argument is the endpoint URI
 * which messages will be published to, and is required.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.pub("inproc://events")
 *     sock:send('keyup', 'key', 'C')
 */
static int lzmq_pub(lua_State *L) {
    const char *endpoint = lua_tostring(L, 1);
    lsock_t *sock = (lsock_t *) lua_newuserdata(L, sizeof(lsock_t));
    luaL_getmetatable(L, MT_ZSOCK);
    lua_setmetatable(L, -2);
    sock->zsock = zsock_new_pub(endpoint);
    sock->poller = zpoller_new(sock->zsock, NULL);
    sock->as_coroutine = 0;
    sock->human = (char *) calloc(strlen(endpoint) + 31, sizeof(char));
    sprintf(sock->human, "<zsock:0x%08" PRIxPTR " pub:%s>", (uintptr_t) sock->zsock, endpoint);
    LDEBUG("lua: zmq: creating socket: %s", sock->human);
    if (!sock->zsock) return Lzmq_push_error(L);
    return 1;
}

/*
 * Creates a new `REQ` (request) socket. The argument is the
 * endpoint URI and is required.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.req("inproc://settings")
 */
static int lzmq_req(lua_State *L) {
    const char *endpoint = lua_tostring(L, 1);
    lsock_t *sock = (lsock_t *) lua_newuserdata(L, sizeof(lsock_t));
    luaL_getmetatable(L, MT_ZSOCK);
    lua_setmetatable(L, -2);
    sock->zsock = zsock_new_req(endpoint);
    sock->poller = zpoller_new(sock->zsock, NULL);
    sock->as_coroutine = 0;
    sock->human = (char *) calloc(strlen(endpoint) + 31, sizeof(char));
    sprintf(sock->human, "<zsock:0x%08" PRIxPTR " req:%s>", (uintptr_t) sock->zsock, endpoint);
    LDEBUG("lua: zmq: creating socket: %s", sock->human);
    if (!sock->zsock) return Lzmq_push_error(L);
    return 1;
}

/*
 * Creates a new `REP` (reply) socket. The argument is the
 * endpoint URI and is required.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.rep("inproc://api")
 */
static int lzmq_rep(lua_State *L) {
    const char *endpoint = lua_tostring(L, 1);
    lsock_t *sock = (lsock_t *) lua_newuserdata(L, sizeof(lsock_t));
    luaL_getmetatable(L, MT_ZSOCK);
    lua_setmetatable(L, -2);
    sock->zsock = zsock_new_rep(endpoint);
    sock->poller = zpoller_new(sock->zsock, NULL);
    sock->as_coroutine = 0;
    sock->human = (char *) calloc(strlen(endpoint) + 31, sizeof(char));
    sprintf(sock->human, "<zsock:0x%08" PRIxPTR " rep:%s>", (uintptr_t) sock->zsock, endpoint);
    LDEBUG("lua: zmq: creating socket: %s", sock->human);
    if (!sock->zsock) return Lzmq_push_error(L);
    return 1;
}

/*
 * Creates a new `PAIR` socket. The argument is the endpoint URI and is
 * required.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.pair("inproc://api")
 */
static int lzmq_pair(lua_State *L) {
    const char *endpoint = lua_tostring(L, 1);
    lsock_t *sock = (lsock_t *) lua_newuserdata(L, sizeof(lsock_t));
    luaL_getmetatable(L, MT_ZSOCK);
    lua_setmetatable(L, -2);
    sock->zsock = zsock_new_pair(endpoint);
    sock->poller = zpoller_new(sock->zsock, NULL);
    sock->as_coroutine = 0;
    sock->human = (char *) calloc(strlen(endpoint) + 31, sizeof(char));
    sprintf(sock->human, "<zsock:0x%08" PRIxPTR " pair:%s>", (uintptr_t) sock->zsock, endpoint);
    LDEBUG("lua: zmq: creating socket: %s", sock->human);
    if (!sock->zsock) return Lzmq_push_error(L);
    return 1;
}

/*
 * Closes and frees this socket.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.req("inproc://settings")
 *     sock:close()
 */
static int socket_close(lua_State *L) {
    lsock_t *s = luaL_checkudata(L, 1, MT_ZSOCK);
    LDEBUG("lua: zmq: destroying socket: %s", s->human);
    if (s->zsock) {
        zsock_set_linger(s->zsock, 0);
        zpoller_destroy(&(s->poller));
        zsock_destroy(&(s->zsock));
        free(s->human);
        s->zsock = NULL;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int poll(lua_State *L, zpoller_t *poller, long timeout) {
    zmsg_t *msg = NULL;
    int num_frames = 0;
    void *in = zpoller_wait(poller, timeout);
    if (in) {
        msg = zmsg_recv(in);
        num_frames = zmsg_size(msg);
        while (zmsg_size(msg) > 0) {
            zframe_t *frame = zmsg_pop(msg);
            lua_pushlstring(L, (char *) zframe_data(frame), zframe_size(frame));
            zframe_destroy(&frame);
        }
        zmsg_destroy(&msg);
    } else if (!zpoller_expired(poller)) {
        luaL_error(L, "zmq: interrupted");
    }
    return num_frames;
}

static int socket_recv(lua_State *L);
static int recv_again(lua_State *L, int status, lua_KContext arg) {
    return socket_recv(L);
}

/*
 * Receives data on this socket. If more than 1 frame is
 * received, multiple values will be returned. The return
 * values are always strings, or `nil` if there was no
 * data to receive.
 *
 * This function is non-blocking by default. To perform
 * a blocking read, pass a timeout in milliseconds or `-1`
 * to block forever until data is received.
 *
 * Note: even if you block forever, the return value may
 * still be `nil` if a read error or interrupt occurs.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.sub("inproc://events")
 *     evt = {sock:recv()}     -- nonblocking
 *     evt = {sock:recv(100)}) -- blocks for 100ms
 *     evt = {sock:recv(-1)})  -- blocks forever
 */
static int socket_recv(lua_State *L) {
    lsock_t *sock = (lsock_t *) luaL_checkudata(L, 1, MT_ZSOCK);
    long timeout = (long) lua_tonumber(L, 2);
    int num_frames = 0;

    if ((num_frames = poll(L, sock->poller, 0)) == 0) {
        // no immediate messages, should we wait for a timeout?
        // ...yes, unless as_coroutine is true, in which case we should
        // yield instead.
        if (sock->as_coroutine) {
            return lua_yieldk(L, 0, 0, recv_again);
        } else {
            num_frames = poll(L, sock->poller, timeout);
        }
    }

    return num_frames;
}

/*
 * Sends data on this socket. If more than 1 frame is
 * to be sent, each frame should be sent as a separate
 * argument.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.req("inproc://events")
 *     sock:send("frame1", "frame2", "frame3")
 */
static int socket_send(lua_State *L) {
    lsock_t *sock = (lsock_t *) luaL_checkudata(L, 1, MT_ZSOCK);
//    long timeout = (long) luaL_checkinteger(L, 2);
    int n = lua_gettop(L), i;

    zmsg_t *msg = zmsg_new();
    for (i = 2; i <= n; i++) {
        const char *part = lua_tostring(L, i);
        zmsg_addstr(msg, part == NULL ? "" : part);
    }
    // zmsg_print(msg);
    zmsg_send(&msg, sock->zsock);
    return 0;
}

/*
 * Returns a human-readable string representation of this socket.
 * 
 * Examples:
 * 
 *     zmq = require("lzmq")
 *     sock = zmq.req("inproc://events")
 *     -- these all do the same thing
 *     print(tostring(sock))
 *     print(sock)
 *     print(sock:__tostring())
 */
static int socket_tostring(lua_State *L) {
    lsock_t *sock = (lsock_t *) luaL_checkudata(L, 1, MT_ZSOCK);
    lua_pushstring(L, sock->human);
    return 1;
}

/*
 * Whenever receiving or sending data would cause ZeroMQ to block, if this
 * method has been called, `coroutine.yield()` will be called instead. When
 * the coroutine containing this socket is resumed, the same process will
 * repeat. That is, if no data is ready yet, `coroutine.yield()` will be
 * called again immediately.
 *
 * This pattern can be useful for breaking out of a logical program flow in
 * order to un-block the ZeroMQ socket (by sending or receiving data as
 * necessary) before resuming where execution was stopped.
 *
 * This method is helpful during testing, but likely has limited utility
 * outside of a controlled environment.
 *
 * Examples:
 *
 *     zmq = require("lzmq")
 *     req = zmq.req("inproc://events"):as_coroutine()
 *     rep = zmq.rep("inproc://events")
 *     co = coroutine.create(function()
 *       req:send("request")
 *       msg = req:recv()        -- if this would block, yields instead
 *       print(msg)
 *     end)
 *
 *     co.resume()               -- start the coroutine
 *     rep:send("response data") -- send response to the coroutine's request
 *     co.resume()               -- resume the coroutine to print the output
 *
 */
static int socket_as_coroutine(lua_State *L) {
    lsock_t *sock = (lsock_t *) luaL_checkudata(L, 1, MT_ZSOCK);
    sock->as_coroutine = 1;
    lua_pushvalue(L, 1);
    return 1;
}

static const luaL_Reg zsock_methods[] = {
    {"__gc",         socket_close},
    {"__tostring",   socket_tostring},
    {"close",        socket_close},
    {"recv",         socket_recv},
    {"send",         socket_send},
    {"as_coroutine", socket_as_coroutine},
    {NULL,           NULL}
};

static const luaL_Reg zsock_new_methods[] = {
    {"sub",   lzmq_sub},
    {"pub",   lzmq_pub},
    {"req",   lzmq_req},
    {"rep",   lzmq_rep},
    {"pair",  lzmq_pair},
    {NULL,    NULL}
};

#define set_zmq_const(s) lua_pushinteger(L,ZMQ_##s); lua_setfield(L, -2, #s);

LUALIB_API int luaopen_zmq(lua_State *L)
{
    /* socket metatable. */
    luaL_newmetatable(L, MT_ZSOCK);
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, zsock_methods, 0);

    lua_newtable(L);
    luaL_setfuncs(L, zsock_new_methods, 0);

    return 1;
}

int init_zmq_lua(lua_State *L) {
    // Get package.preload so we can store builtins in it.
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "preload");
    lua_remove(L, -2); // Remove package

    // Store CTOS module definition at preload.CTOS
    lua_pushcfunction(L, luaopen_zmq);
    lua_setfield(L, -2, "lzmq");

    return 0;
}

void shutdown_zmq_lua(lua_State *L) {
    (void) L;
}
