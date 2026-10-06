add_executable(supergame-shared-tests
    test_coop_sim.cpp
    test_pvp_sim.cpp
)

target_link_libraries(supergame-shared-tests PRIVATE
    supergame-shared
    supergame-test-harness
    supergame-warnings
)

# One CTest entry for the whole binary rather than one per case: the harness
# already reports per-case pass/fail on stdout, and a single test keeps
# `ctest` output readable while still failing the build on any bad check.
add_test(NAME shared-sim COMMAND supergame-shared-tests)
