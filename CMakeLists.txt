cmake_minimum_required(VERSION 3.20)
project(supergame VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Warning flags for this project's own three targets. Carried on an INTERFACE
# target that each target links, rather than a directory-scoped
# add_compile_options(), for two reasons: the vendored subdirectories added
# below (raylib, uWebSockets, ixwebsocket) never inherit them - their warnings
# aren't ours to fix and would bury our own - and the flag list lives in one
# place instead of being repeated in every subdirectory's CMakeLists.
#
# Deliberately NOT -Werror yet: the existing sources still emit warnings, so
# a hard error would block every build. Once that list is clear, adding
# -Werror here turns any new warning into a build failure.
add_library(supergame-warnings INTERFACE)
if(MSVC)
    target_compile_options(supergame-warnings INTERFACE /W4)
else()
    target_compile_options(supergame-warnings INTERFACE -Wall -Wextra)
endif()

# Unit tests cover shared/ only - it's the headless, engine-agnostic half of
# the codebase, so the tests need neither raylib nor a running server. Off in
# the Docker image, which only wants the server binary.
option(BUILD_TESTS "Build the supergame unit tests" ON)

# The raylib client pulls in GUI/graphics system deps (X11, OpenGL, ALSA...)
# that the headless server has no use for. Turn it off to configure a
# server-only build (CI server job, Docker image) without needing any of
# that installed.
option(BUILD_CLIENT "Build the raylib-based game client" ON)

if(BUILD_CLIENT)
    # Raylib
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

    add_subdirectory(vendor/raylib)

    # IXWebSocket (client-side WebSocket connection)
    include(vendor/ixwebsocket.cmake)
endif()

# uSockets / uWebSockets (server networking layer)
include(vendor/uWebSockets.cmake)

# nlohmann/json (message serialization)
include(vendor/json.cmake)

#Targets
add_subdirectory(shared)
if(BUILD_CLIENT)
    add_subdirectory(client)
endif()
add_subdirectory(server)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(shared/tests)
endif()
