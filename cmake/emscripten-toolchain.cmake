# WebAssembly via Emscripten (Phase 5). A thin wrapper around Emscripten's own
# toolchain file, so the `web` preset needs no machine-specific path.
#
# Compiler: provided by this repo's flake.nix, as the `web` devShell (CI uses
# the emscripten/emsdk image of the same version).
#
#   nix develop .#web --command cmake --preset web
#   nix develop .#web --command cmake --build --preset web
#
# Emscripten.cmake sits next to `emcc` in an emsdk checkout, and under
# share/emscripten in nix's package, whose bin/emcc is a wrapper script.

find_program(SUPERGAME_EMCC NAMES emcc)

if(NOT SUPERGAME_EMCC)
    message(FATAL_ERROR
        "Emscripten (emcc) not found on PATH. Enter this repo's web shell:\n"
        "  nix develop .#web --command cmake --preset web")
endif()

get_filename_component(_supergame_emcc_dir "${SUPERGAME_EMCC}" REALPATH)
get_filename_component(_supergame_emcc_dir "${_supergame_emcc_dir}" DIRECTORY)

foreach(_root "${_supergame_emcc_dir}" "${_supergame_emcc_dir}/../share/emscripten")
    if(EXISTS "${_root}/cmake/Modules/Platform/Emscripten.cmake")
        include("${_root}/cmake/Modules/Platform/Emscripten.cmake")
        return()
    endif()
endforeach()

message(FATAL_ERROR
    "Found emcc at ${SUPERGAME_EMCC}, but not Emscripten.cmake beside it or "
    "under ../share/emscripten.")
