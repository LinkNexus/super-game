add_executable(supergame-server-tests
    test_roster.cpp
    test_match.cpp
)

target_link_libraries(supergame-server-tests PRIVATE
    supergame-server-lib
    supergame-test-harness
    supergame-warnings
)

target_include_directories(supergame-server-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Separate CTest entry from `shared-sim` so a roster/lifetime regression
# reports distinctly from a simulation one.
add_test(NAME server-lifetime COMMAND supergame-server-tests)
