# The harness is its own target rather than two files compiled into each test
# binary, because there are now two test executables - `supergame-shared-tests`
# for the headless sim and `supergame-server-tests` for the server's
# lifetime/matchmaking logic (L71) - and both need the same runner.
#
# `main()` living in a static library is the same arrangement gtest_main uses:
# the C runtime's startup object carries an undefined reference to `main`, so
# the linker pulls this object in, and the test translation units reference
# registry()/recordCheck() from it anyway.
#
# Kept outside both shared/ and server/ since it belongs to neither - it has no
# dependency on either target and must not grow one.
add_library(supergame-test-harness STATIC
    test_main.cpp
)

target_include_directories(supergame-test-harness PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(supergame-test-harness PRIVATE supergame-warnings)
