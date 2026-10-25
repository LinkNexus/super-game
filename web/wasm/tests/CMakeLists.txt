# Loads the built module in node and drives it through the JS API, so the
# Embind binding itself is covered, not just shared/ (that's shared-sim).
# CMAKE_CROSSCOMPILING_EMULATOR is node, set by Emscripten's toolchain file.
add_test(NAME web-sim-binding
    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
        ${CMAKE_CURRENT_SOURCE_DIR}/test_web_sim.mjs
        $<TARGET_FILE:supergame-web-sim>
)
# node has (rarely, ~3 in 600 local runs on macOS, cause not found) hung in
# process.exit() after every check passed; fail fast instead of stalling CI.
set_tests_properties(web-sim-binding PROPERTIES TIMEOUT 60)
