add_library(supergame-shared STATIC
    src/bullet_sim.cpp
    src/enemy_sim.cpp
    src/player_sim.cpp
    src/boss_sim.cpp
)

target_include_directories(supergame-shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
