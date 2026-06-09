# Space Game

## Requirements

- CMake ≥ 3.20
- Ninja (`winget install Ninja-build.Ninja` on Windows)
- A C++17 compiler (MSVC / Clang on Windows, GCC/Clang on Linux)

## First-time setup

```bash
git clone --recurse-submodules git@gitlab.rz.htw-berlin.de:s0594529/super_game.git
cd super_game
```

If you forgot `--recurse-submodules`:
```bash
git submodule update --init --recursive
```

## Build & run

```bash
cmake --preset debug
cmake --build build/debug
./build/debug/bin/supergame-client      # Linux
build\debug\bin\supergame-client.exe    # Windows
```
