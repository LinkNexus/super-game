# nlohmann/json is vendored as a single header (not a full git submodule) -
# https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
# There's no build step for a header-only library, so this just exposes an
# INTERFACE target with the include path, same shape as the `uwebsockets`
# target in uWebSockets.cmake.

add_library(nlohmann_json INTERFACE)

# SYSTEM for the same reason as the uwebsockets target: json.hpp is a 25k-line
# vendored header compiled into nearly every translation unit we have, and its
# warnings aren't ours to fix. Needed before -Werror can go on (L64).
target_include_directories(nlohmann_json SYSTEM INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/vendor/json/include
)
