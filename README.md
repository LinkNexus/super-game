# Space Game

## Requirements

- CMake ≥ 3.20
- Ninja (`winget install Ninja-build.Ninja` on Windows)
- A C++20 compiler (MSVC / Clang on Windows, GCC/Clang on Linux)

## First-time setup

```bash
git clone git@github.com:LinkNexus/super-game.git
cd super-game
git submodule update --init vendor/raylib vendor/uWebSockets vendor/ixwebsocket vendor/mbedtls
git -C vendor/uWebSockets submodule update --init uSockets
git -C vendor/mbedtls submodule update --init framework
```

Do **not** use `--recurse-submodules` / `--recursive` here: `uWebSockets` and `uSockets`
bundle their own submodules for SSL, HTTP/3, and fuzz-testing (BoringSSL, lsquic, a fuzzing
corpus) that together are over 1 GB and that this project doesn't use — the server runs
plain `ws://` with no compression. The commands above fetch only `uSockets` and mbedTLS's
`framework` submodule (needed just to configure mbedTLS, even though we don't build its
tests), which is all the build needs.

## Build & run

```bash
make                    # everything, Debug
make BUILD=release      # everything, Release
```

Each module also builds on its own, into its own `build/<preset>` tree, so
switching between them never reconfigures another one's cache:

```bash
make client             # just the raylib client   (no server, no tests)
make server             # just the headless server (no raylib)
make tests              # just the two test suites (no raylib, no uWebSockets)

make server BUILD=release
make run-client         # builds, then runs against ws://localhost:9001
make run-server
```

`make help` lists the targets. Every target is a thin wrapper over a CMake
preset of the same name (`client`, `client-release`, `server`,
`server-release`, `tests`), so the equivalent without `make` is:

```bash
cmake --preset server-release
cmake --build --preset server-release
./build/server-release/bin/supergame-server
```

`cmake --list-presets` shows all of them. Which modules a preset builds is
controlled by the `BUILD_CLIENT` / `BUILD_SERVER` / `BUILD_TESTS` options, so
any other combination works too:

```bash
cmake -S . -B build/mine -G Ninja -DBUILD_CLIENT=OFF -DBUILD_TESTS=OFF
```

## Tests

```bash
make test
```

That configures and builds a headless `tests` preset (no raylib, no client, and with
`BUILD_SERVER=OFF` no uWebSockets either - so it needs **no submodules at all**) and runs both
suites through CTest — the simulation in `shared/tests/` and the server's matchmaking,
rosters and broadcast loop in `server/tests/`. Neither needs a graphics stack or a listening
port, so this also works on Windows, where the server *executable* isn't buildable.

Without `make`, or to run a single suite:

```bash
cmake --preset tests && cmake --build --preset tests
ctest --preset tests                      # both suites
./build/tests/bin/supergame-shared-tests  # one suite, per-case output
./build/tests/bin/supergame-server-tests
```

`make test-debug` runs the same suites against an existing full `build/debug` tree instead
of a second build directory.
