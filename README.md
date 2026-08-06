# Space Game

## Requirements

- CMake ≥ 3.20
- Ninja (`winget install Ninja-build.Ninja` on Windows)
- A C++17 compiler (MSVC / Clang on Windows, GCC/Clang on Linux)

## First-time setup

```bash
git clone git@gitlab.rz.htw-berlin.de:s0594529/super_game.git
cd super_game
git submodule update --init vendor/raylib vendor/uWebSockets vendor/ixwebsocket
git -C vendor/uWebSockets submodule update --init uSockets
```

Do **not** use `--recurse-submodules` / `--recursive` here: `uWebSockets` and `uSockets`
bundle their own submodules for SSL, HTTP/3, and fuzz-testing (BoringSSL, lsquic, a fuzzing
corpus) that together are over 1 GB and that this project doesn't use — the server runs
plain `ws://` with no compression. The commands above fetch only `uSockets`, which is all
the build needs.

## Build & run

```bash
cmake --preset debug
cmake --build build/debug
./build/debug/bin/supergame-client      # Linux
build\debug\bin\supergame-client.exe    # Windows
```
