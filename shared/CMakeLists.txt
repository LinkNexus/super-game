add_library(supergame-shared STATIC
    src/sim/bullet.cpp
    src/sim/enemies.cpp
    src/sim/player.cpp
    src/sim/boss.cpp
)

target_include_directories(supergame-shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(supergame-shared
    PUBLIC nlohmann_json mathlib
    PRIVATE supergame-warnings
)
