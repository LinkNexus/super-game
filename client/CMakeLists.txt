add_executable(supergame-client
    src/main.cpp
)

target_link_libraries(supergame-client PRIVATE raylib)

target_include_directories(supergame-client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/shared          
)

# Windows: link required system libs (Raylib needs these)
if(WIN32)
    target_link_libraries(supergame-client PRIVATE winmm)
endif()

# Copy assets next to the binary after build
add_custom_command(TARGET supergame-client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:supergame-client>/assets
    COMMENT "Copying assets..."
)
