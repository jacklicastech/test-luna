-- See the documentation at
-- https://github.com/appilee/luna/blob/master/src/plugins/backend.c for more
-- details about how this process is intended to work.

local events = require('events')
local inspect = require('inspect')
local JSON = require('rest-api/mime-type/json')
local running = true

-- create a socket which will be used to begin the request
local backend = require('lzmq').req('inproc://backend')

-- start the request. This runs in the background on a separate thread, so
-- lua execution will proceed immediately.
backend:send('url', 'https://www.google.com/api/v1/ping.json',
             'method', 'GET',
             'Content-type', 'application/json',
             'body', JSON:encode({ping = true}))

-- receive topic, which is always 'broadcast_id', and the request_id, which
-- is the important information. The socket takes a timeout, and -1 means to
-- wait forever. In practice you should receive a reply to this request almost
-- immediately. The reply does not mean the HTTPS request has finished
-- running, it is just an acknowledgement that the request is enqueued, and
-- the ID is used by you to wait for the HTTPS response data.
local topic, request_id = backend:recv(-1)

-- register to wait for the information associated with request_id via the
-- events system
local listener = { trigger = function(evt)
  -- when this function is called, evt will contain all the information about
  -- the HTTPS response.
  print(inspect(evt))

  -- remove the listener to prevent leaks, because we are done with the
  -- request_id.
  events.focus(nil, 'backend-complete ' .. request_id)

  -- end the test program
  running = false
end }

-- register the above listener to receive the HTTPS response when it is ready
events.focus(listener, 'backend-complete ' .. request_id)

-- loop until the HTTPS request completes
while running do
  events.tick()
end
