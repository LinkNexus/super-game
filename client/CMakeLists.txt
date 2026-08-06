add_executable(supergame-client
    src/main.cpp
    src/game.cpp
    src/entities/player.cpp
    src/entities/enemy.cpp
    src/entities/boss.cpp
    src/entities/star.cpp
    src/network_client.cpp
    src/session.cpp
)

target_link_libraries(supergame-client PRIVATE raylib supergame-shared ixwebsocket)

target_include_directories(supergame-client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Windows: link required system libs (Raylib needs these)
if(WIN32)
    target_link_libraries(supergame-client PRIVATE winmm)

    # windows.h (pulled in transitively via ixwebsocket's winsock2 usage)
    # declares User32/GDI functions (DrawTextA, Rectangle, CloseWindow,
    # ShowCursor...) that collide by name with raylib's own API. We never
    # call the Win32 GDI/User32 APIs directly - raylib owns the window via
    # GLFW - so it's safe to tell windows.h to skip declaring them.
    target_compile_definitions(supergame-client PRIVATE NOGDI NOUSER)

    # Statically link the MinGW runtime (libgcc/libstdc++/libwinpthread) so
    # the .exe runs standalone - a stock Windows install doesn't have those
    # DLLs, only the Universal CRT ones.
    target_link_options(supergame-client PRIVATE -static-libgcc -static-libstdc++ -static)

    # No console window alongside the game window when launched.
    set_target_properties(supergame-client PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Copy assets next to the binary after build
add_custom_command(TARGET supergame-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:supergame-client>/assets
    COMMENT "Copying assets..."
)
