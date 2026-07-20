# Builds uSockets (uWebSockets' transport layer) as a static library and
# exposes an INTERFACE target for uWebSockets' header-only C++ API.
#
# We deliberately build without SSL/QUIC/zlib support: the game server speaks
# plain ws:// (any TLS termination happens at the reverse proxy in
# deployment) and doesn't need permessage-deflate compression. This keeps the
# dependency footprint to just uSockets' own sources - no OpenSSL, no zlib,
# no BoringSSL/lsquic.

set(USOCKETS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor/uWebSockets/uSockets)

file(GLOB USOCKETS_SOURCES
    ${USOCKETS_DIR}/src/*.c
    ${USOCKETS_DIR}/src/eventing/*.c
    ${USOCKETS_DIR}/src/crypto/*.c
    ${USOCKETS_DIR}/src/io_uring/*.c
)

add_library(usockets STATIC ${USOCKETS_SOURCES})

target_include_directories(usockets PUBLIC
    ${USOCKETS_DIR}/src
)

target_compile_definitions(usockets PUBLIC
    LIBUS_NO_SSL
)

set_target_properties(usockets PROPERTIES C_STANDARD 11)

add_library(uwebsockets INTERFACE)

target_include_directories(uwebsockets INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/uWebSockets/src
)

target_compile_definitions(uwebsockets INTERFACE
    UWS_NO_ZLIB
)

target_link_libraries(uwebsockets INTERFACE usockets)
