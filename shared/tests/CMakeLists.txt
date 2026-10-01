add_executable(supergame-tests
    test_main.cpp
    test_coop_sim.cpp
    test_pvp_sim.cpp
)

target_link_libraries(supergame-tests PRIVATE supergame-shared supergame-warnings)

target_include_directories(supergame-tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# One CTest entry for the whole binary rather than one per case: the harness
# already reports per-case pass/fail on stdout, and a single test keeps
# `ctest` output readable while still failing the build on any bad check.
add_test(NAME shared-sim COMMAND supergame-tests)
