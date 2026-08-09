cmake_minimum_required(VERSION 3.20)
project(supergame VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

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
