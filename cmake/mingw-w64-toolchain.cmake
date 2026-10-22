# Cross-compile toolchain: Windows x86_64 via MinGW-w64.
#
# Used to build a Windows supergame-client.exe from the Linux CI runner (or
# any Linux/macOS dev machine) - no Windows machine needed. raylib and
# ixwebsocket both already detect WIN32/mingw in their own CMake and link
# the right system libs (opengl32/gdi32/winmm, ws2_32/wsock32/shlwapi).
#
# Compiler: provided by this repo's flake.nix, as the `mingw` devShell.
#
#   nix develop .#mingw --command cmake --preset windows-client-release
#   nix develop .#mingw --command cmake --build --preset windows-client-release
#
# Do NOT install the cross compiler globally instead (apt
# g++-mingw-w64-x86-64, brew mingw-w64, or nix
# pkgsCross.mingwW64.buildPackages.gcc in a system profile). With nix at
# least, that reliably fails in CMake's compiler check below with
#
#   cannot find -lmcfgthread
#
# which looks like a broken install and is not one. nixpkgs builds
# mingwW64 GCC with --enable-threads=mcf, so the driver implicitly links
# mcfgthread and the C++ headers include <mcfgthread/gthr.h>; the paths to
# both come from environment variables exported by nix's stdenv setup
# hook. A bare compiler on PATH has never had those exported, so it
# cannot find a library that is sitting right there in the store. Inside
# the devShell the setup hook has run and the identical compiler works.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(MINGW_PREFIX x86_64-w64-mingw32)

find_program(CMAKE_C_COMPILER   NAMES ${MINGW_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${MINGW_PREFIX}-g++)
find_program(CMAKE_RC_COMPILER  NAMES ${MINGW_PREFIX}-windres)

if(NOT CMAKE_C_COMPILER OR NOT CMAKE_CXX_COMPILER)
    message(FATAL_ERROR
        "MinGW-w64 cross compiler (${MINGW_PREFIX}-gcc/g++) not found on PATH. "
        "Enter this repo's cross-compilation shell rather than installing "
        "the compiler globally -- see the note at the top of this file:\n"
        "  nix develop .#mingw --command cmake --preset windows-client-release")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
