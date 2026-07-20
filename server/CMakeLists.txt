add_executable(supergame-server
    src/main.cpp
)

target_link_libraries(supergame-server PRIVATE supergame-shared)

target_include_directories(supergame-server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
