# IXWebSocket provides the WebSocket *client* that uWebSockets doesn't - its
# own examples/Client.cpp is an explicit stub ("There is no client support
# implemented in the library"). Used by client/ only; server/ keeps using
# uWebSockets.
#
# TLS is on (via vendored mbedTLS, see vendor/mbedtls.cmake) since the
# deployed server sits behind Cloudflare/NPM and needs wss://, not ws://.
# zlib stays off - no permessage-deflate/compression needed either way.

include(${CMAKE_CURRENT_SOURCE_DIR}/vendor/mbedtls.cmake)

set(USE_TLS ON CACHE BOOL "" FORCE)
set(USE_MBED_TLS ON CACHE BOOL "" FORCE)
set(USE_ZLIB OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
# We never `cmake --install` this project - skip ixwebsocket's own
# install(EXPORT ...) step, which would otherwise fail validation since our
# vendored mbedtls/mbedx509/mbedcrypto targets aren't part of any export set.
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/ixwebsocket)
