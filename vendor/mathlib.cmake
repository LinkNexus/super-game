# mathlib-cpp (https://github.com/LinkNexus/mathlib-cpp): header-only Vec2/Vec3,
# vendored as a git submodule and added through its own CMakeLists, which
# defines the `mathlib` INTERFACE target. As a subproject it builds no tests,
# defines no `check` target and leaves the build type alone.

if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/vendor/mathlib/CMakeLists.txt)
    message(FATAL_ERROR
        "vendor/mathlib is empty - run `git submodule update --init vendor/mathlib`")
endif()

# Already its default as a subproject; spelled out so a cached ON can't sneak in.
set(MATHLIB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# SYSTEM: its warnings aren't ours to fail -Werror on (see json.cmake).
add_subdirectory(vendor/mathlib SYSTEM)
