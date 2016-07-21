-- Confirm that lr can run modules installed on the system
-- Install a lua-sockets package and give this try:
--      lr ./testing/grab.lua --url="http://www.google.com"

if not (url or arg[1]) then return; end

local io = require("io")
local http = require("socket.http")
local ltn12 = require("ltn12")

http.request{ 
   url = arg[1] or url,
   sink = ltn12.sink.file(io.stdout)
}

