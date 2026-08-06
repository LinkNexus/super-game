# IXWebSocket provides the WebSocket *client* that uWebSockets doesn't - its
# own examples/Client.cpp is an explicit stub ("There is no client support
# implemented in the library"). Used by client/ only; server/ keeps using
# uWebSockets.
#
# Built without TLS/zlib, matching uWebSockets' UWS_NO_ZLIB setup: the game
# speaks plain ws://, so there's no permessage-deflate or wss:// to support,
# and this keeps the dependency footprint down (no OpenSSL/zlib needed to
# build on a fresh machine, Windows included).

set(USE_TLS OFF CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/ixwebsocket)
