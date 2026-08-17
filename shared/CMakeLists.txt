add_library(supergame-shared STATIC
    src/sim/bullet_sim.cpp
    src/sim/enemy_sim.cpp
    src/sim/player_sim.cpp
    src/sim/boss_sim.cpp
    src/sim/game_sim.cpp
    src/math_utils.cpp
    src/rnd_generator.cpp
    src/modes.cpp
)

target_include_directories(supergame-shared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(supergame-shared PUBLIC nlohmann_json)
