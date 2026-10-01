add_executable(supergame-server
    src/main.cpp
    src/game.cpp
)

target_link_libraries(supergame-server PRIVATE supergame-shared uwebsockets supergame-warnings)

target_include_directories(supergame-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
