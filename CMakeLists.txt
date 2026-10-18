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
# -Werror is on (L64): the warning list is clear, so any new warning in our own
# code fails the build rather than scrolling past. Two things make that safe to
# leave on. Vendored include paths are marked SYSTEM (see
# supergame_mark_vendored_system below, plus the uwebsockets and nlohmann_json
# targets), so a warning in raylib.h or json.hpp compiled inside one of our
# translation units is not ours to fail on. And vendored *targets* never link
# this interface, so raylib compiling its own stb_vorbis.c is unaffected.
#
# If a new warning ever has to be tolerated temporarily, suppress that one
# diagnostic (-Wno-<name>) with a comment rather than dropping -Werror
# globally - the whole value here is that the clean state stays clean.
add_library(supergame-warnings INTERFACE)
if(MSVC)
    target_compile_options(supergame-warnings INTERFACE /W4 /WX)
else()
    target_compile_options(supergame-warnings INTERFACE -Wall -Wextra -Werror)
endif()

# Marks a vendored target's interface include directories as SYSTEM, so that
# -Wall -Wextra (and, once L64 lands, -Werror) stay off *their* headers when
# those headers are compiled as part of *our* translation units.
#
# supergame-warnings already keeps our flags off vendored *targets* - but a
# vendored header pulled into one of our .cpp files is compiled with our flags,
# so the suppression has to travel with the include path instead. Without this,
# -Werror would fail on warnings in raylib.h or IXWebSocket.h that aren't ours
# to fix.
#
# `add_subdirectory(... SYSTEM)` and the SYSTEM target property both do this in
# one line, but they are CMake 3.25+ and this project's floor is 3.20 (see
# cmake_minimum_required above), so copy the interface includes across by hand.
# Targets we define ourselves (uwebsockets, nlohmann_json) pass SYSTEM straight
# to target_include_directories and don't need this.
function(supergame_mark_vendored_system target)
    if(NOT TARGET ${target})
        message(AUTHOR_WARNING
            "supergame_mark_vendored_system: no target '${target}'")
        return()
    endif()

    get_target_property(_aliased ${target} ALIASED_TARGET)
    if(_aliased)
        set(target ${_aliased})
    endif()

    get_target_property(_includes ${target} INTERFACE_INCLUDE_DIRECTORIES)
    if(_includes)
        set_target_properties(${target} PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_includes}")
    endif()
endfunction()

# Unit tests cover the two headless halves of the codebase: shared/ (the
# simulation) and server/ (matchmaking and connection lifetime, via
# supergame-server-lib). Neither needs raylib or a listening socket, so the
# test build stays free of the client's graphics deps. Off in the Docker image,
# which only wants the server binary.
option(BUILD_TESTS "Build the supergame unit tests" ON)

# The raylib client pulls in GUI/graphics system deps (X11, OpenGL, ALSA...)
# that the headless server has no use for. Turn it off to configure a
# server-only build (CI server job, Docker image) without needing any of
# that installed.
option(BUILD_CLIENT "Build the raylib-based game client" ON)

# The server is not buildable on Windows without an extra dependency we don't
# vendor. uSockets picks its event backend at compile time from the target
# platform (vendor/uWebSockets/uSockets/src/libusockets.h): kqueue on
# macOS/BSD, epoll on Linux - both kernel facilities needing no library - but
# **libuv on Windows**. With `_WIN32` defined, `internal/internal.h` pulls in
# `internal/eventing/libuv.h`, which `#include <uv.h>`, and the build fails
# there.
#
# This project only ever deploys the server as a Linux container, and the
# Windows target exists solely to produce a client .exe, so the server (and
# with it the whole uSockets build) is off by default on Windows. Turning it
# back on there means also vendoring libuv and defining LIBUS_USE_LIBUV.
#
# Note this has to gate `include(vendor/uWebSockets.cmake)` too, not just the
# server target: `add_library(usockets STATIC ...)` is part of the default
# `all` target, so a plain `cmake --build <dir>` would compile libuv.c and hit
# the same error even when supergame-server itself is never requested.
if(WIN32)
    set(SUPERGAME_BUILD_SERVER_DEFAULT OFF)
else()
    set(SUPERGAME_BUILD_SERVER_DEFAULT ON)
endif()
option(BUILD_SERVER "Build the headless game server"
       ${SUPERGAME_BUILD_SERVER_DEFAULT})

if(WIN32 AND NOT BUILD_SERVER)
    message(STATUS
        "supergame-server: skipped (Windows target - uSockets would need "
        "libuv here; the server ships as a Linux container)")
endif()

if(BUILD_CLIENT)
    # Raylib
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

    add_subdirectory(vendor/raylib)
    supergame_mark_vendored_system(raylib)

    # IXWebSocket (client-side WebSocket connection)
    include(vendor/ixwebsocket.cmake)
endif()

# uSockets / uWebSockets (server networking layer)
if(BUILD_SERVER)
    include(vendor/uWebSockets.cmake)
endif()

# nlohmann/json (message serialization)
include(vendor/json.cmake)

#Targets
add_subdirectory(shared)
if(BUILD_CLIENT)
    add_subdirectory(client)
endif()
# server/CMakeLists.txt gates only the *executable* on BUILD_SERVER. The
# library half (matchmaking, rosters, the sim-stepping broadcast loop) names no
# uWS type, so it builds everywhere and its tests come along - see the comment
# on the add_executable there. That is why BUILD_TESTS alone is enough to want
# this subdirectory: the `tests` preset builds the server's tests with
# BUILD_SERVER=OFF and so needs no vendored submodule at all.
if(BUILD_SERVER OR BUILD_TESTS)
    add_subdirectory(server)
endif()

if(BUILD_TESTS)
    enable_testing()

    # The runner/assertion macros, shared by both test binaries below.
    add_subdirectory(tests/harness)

    add_subdirectory(shared/tests)

    # Not gated on BUILD_SERVER: these link supergame-server-lib, which is
    # free of uSockets, so they build and run even where the server executable
    # can't (Windows/MinGW).
    add_subdirectory(server/tests)
endif()
