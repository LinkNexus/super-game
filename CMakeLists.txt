cmake_minimum_required(VERSION 3.20)
project(supergame VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Raylib
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

add_subdirectory(vendor/raylib)

# uSockets / uWebSockets (server networking layer)
include(vendor/uWebSockets.cmake)

# nlohmann/json (message serialization)
include(vendor/json.cmake)

# IXWebSocket (client-side WebSocket connection)
include(vendor/ixwebsocket.cmake)

#Targets
add_subdirectory(shared)
add_subdirectory(client)
add_subdirectory(server)
