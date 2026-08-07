# mbedTLS provides the TLS backend for IXWebSocket's wss:// support (the
# deployed server sits behind Cloudflare/NPM, which requires TLS). Built
# from source the same way on every platform (macOS, Linux CI, MinGW
# Windows cross-compile) - no system OpenSSL dependency anywhere, matching
# how uSockets/ixwebsocket/raylib are all vendored+built rather than
# expected to be pre-installed.
#
# IXWebSocket's own CMakeLists.txt normally calls find_package(MbedTLS
# REQUIRED), guarded by `if (NOT MBEDTLS_FOUND)`. Pre-populating
# MBEDTLS_FOUND/MBEDTLS_INCLUDE_DIRS/MBEDTLS_LIBRARIES here (before
# vendor/ixwebsocket.cmake's add_subdirectory) satisfies that guard with
# our own vendored build instead of requiring a system install.

set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/vendor/mbedtls)

set(MBEDTLS_FOUND TRUE)
set(MBEDTLS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mbedtls/include)
set(MBEDTLS_LIBRARIES mbedtls mbedx509 mbedcrypto)
