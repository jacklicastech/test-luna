## lzmq

### lzmq.rep

Creates a new `REP` (reply) socket. The argument is the
endpoint URI and is required.

Examples:

    zmq = require("lzmq")
    sock = zmq.rep("inproc://api")


### lzmq.req

Creates a new `REQ` (request) socket. The argument is the
endpoint URI and is required.

Examples:

    zmq = require("lzmq")
    sock = zmq.req("inproc://settings")


### lzmq.sub

Creates a new `SUB` (subscription) socket. The first argument is the
endpoint URI and is required. The second argument may be a
topic prefix to subscribe to. If omitted, all messages published
to this endpoint will be subscribed to.

Examples:

    zmq = require("lzmq")
    sock = zmq.sub("inproc://events")
    sock = zmq.sub("inproc://events", "keyup")


## socket

### socket.close

Closes and frees this socket.

Examples:

    zmq = require("lzmq")
    sock = zmq.req("inproc://settings")
    sock:close()


### socket.recv

Receives data on this socket. If more than 1 frame is
received, multiple values will be returned. The return
values are always strings, or `nil` if there was no
data to receive.

This function is non-blocking by default. To perform
a blocking read, pass a timeout in milliseconds or `-1`
to block forever until data is received.

Note: even if you block forever, the return value may
still be `nil` if a read error or interrupt occurs.

Examples:

    zmq = require("lzmq")
    sock = zmq.sub("inproc://events")
    evt = {sock:recv()}     -- nonblocking
    evt = {sock:recv(100)}) -- blocks for 100ms
    evt = {sock:recv(-1)})  -- blocks forever


### socket.send

Sends data on this socket. If more than 1 frame is
to be sent, each frame should be sent as a separate
argument.

Examples:

    zmq = require("lzmq")
    sock = zmq.req("inproc://events")
    sock:send("frame1", "frame2", "frame3")


