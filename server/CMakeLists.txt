# The server splits into a library plus a thin executable (L71) so the
# lifetime/matchmaking logic is linkable by something other than `main()`.
# Everything in `game.cpp` - GameManager, CoopGame, PvPGame, the rosters - is
# driven today only by the uWS callbacks, which means the only way to exercise
# a join/disconnect/rejoin ordering was to run the binary and drive real
# WebSocket sockets at it from outside. Two of the worst bugs on the L58-L61
# branch (the `.upgrade` fall-through that killed the process on first
# connection, and `addPlayers` writing at the player count instead of the first
# free slot) were invisible to both the compiler and a careful diff read, and
# both are ordinary unit tests once this code lives in a library.
#
# main.cpp keeps only what genuinely needs the uWS App: the handler wiring, the
# timer, and `listen()`.
add_library(supergame-server-lib STATIC
    src/game.cpp
    src/game_manager.cpp
)

# No uWS here, deliberately. The transport sits behind the `WsConnection`
# interface in include/types.h, so nothing in this library names a uWS type -
# only `main.cpp`'s adapter does. That keeps the test binary free of the
# dependency (and buildable where BUILD_SERVER is off, e.g. Windows); if a
# `#include "App.h"` ever reappears in here, it's a sign the seam has leaked.
#
# supergame-shared is PUBLIC because `include/game.h` exposes shared/ types in
# its own interface, so consumers need those include paths too.
target_link_libraries(supergame-server-lib
    PUBLIC supergame-shared
    PRIVATE supergame-warnings
)

target_include_directories(supergame-server-lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Only the executable is gated on BUILD_SERVER - the library above is not.
# uSockets is what BUILD_SERVER exists to exclude (it needs libuv on Windows),
# and only main.cpp touches it, so the lib and its tests stay available on
# every platform. That's what lets `supergame-server-tests` build and run on a
# Windows/MinGW target where `supergame-server` itself can't.
if(BUILD_SERVER)
    add_executable(supergame-server
        src/main.cpp
    )

    target_link_libraries(supergame-server PRIVATE
        supergame-server-lib
        uwebsockets
        supergame-warnings
    )
endif()
