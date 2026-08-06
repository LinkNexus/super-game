# Cross-compile toolchain: Windows x86_64 via MinGW-w64.
#
# Used to build a Windows supergame-client.exe from the Linux CI runner (or
# any Linux/macOS dev machine) - no Windows machine needed. raylib and
# ixwebsocket both already detect WIN32/mingw in their own CMake and link
# the right system libs (opengl32/gdi32/winmm, ws2_32/wsock32/shlwapi).
#
# Compiler package:
#   Debian/Ubuntu: apt install g++-mingw-w64-x86-64
#   macOS (Homebrew): brew install mingw-w64
#
# Usage: cmake --preset windows-client-release

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_PREFIX x86_64-w64-mingw32)

find_program(CMAKE_C_COMPILER   NAMES ${MINGW_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${MINGW_PREFIX}-g++)
find_program(CMAKE_RC_COMPILER  NAMES ${MINGW_PREFIX}-windres)

if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR
        "MinGW-w64 cross compiler (${MINGW_PREFIX}-gcc/g++) not found on PATH. "
        "Install it (Debian/Ubuntu: apt install g++-mingw-w64-x86-64, "
        "macOS: brew install mingw-w64).")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
