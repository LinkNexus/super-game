.PHONY: all client server tests run run-client run-server \
        clean rebuild test test-asan test-debug help debug release

# Which module to build, and at which optimisation level. Every target below
# resolves to a CMake preset named `<module>` (Debug) or `<module>-release`,
# so `make server BUILD=release` and `cmake --preset server-release` are the
# same build - nothing here knows anything the presets don't.
#
#   make                  # everything, Debug
#   make client           # just the client
#   make server BUILD=release
#   make tests            # just the two headless test suites
#
BUILD ?= debug

ifeq ($(BUILD),release)
SUFFIX := -release
else ifeq ($(BUILD),debug)
SUFFIX :=
else
$(error BUILD must be `debug` or `release`, got `$(BUILD)`)
endif

# `all` keeps its own preset names (debug/release) rather than a module name,
# since it is the one configuration that builds every target at once.
ALL_PRESET := $(if $(SUFFIX),release,debug)

help:
	@echo "Targets:  all client server tests   |   run-client run-server   |   test test-asan clean rebuild"
	@echo "Variable: BUILD=debug (default) | release"
	@echo "Example:  make server BUILD=release"

# --- Whole project --------------------------------------------------------

all:
	cmake --preset $(ALL_PRESET)
	cmake --build --preset $(ALL_PRESET)

# Kept so the old spellings still work; `make` and `make BUILD=release` are
# the same two builds.
debug:
	$(MAKE) all BUILD=debug

release:
	$(MAKE) all BUILD=release

# --- One module at a time -------------------------------------------------
# Each lands in its own build/<preset> tree, so switching modules never
# reconfigures (or invalidates) another one's cache.

client:
	cmake --preset client$(SUFFIX)
	cmake --build --preset client$(SUFFIX)

server:
	cmake --preset server$(SUFFIX)
	cmake --build --preset server$(SUFFIX)

tests:
	cmake --preset tests
	cmake --build --preset tests

# --- Running --------------------------------------------------------------

run: run-client

run-client: client
	./build/client$(SUFFIX)/bin/supergame-client ws://localhost:9001

run-server: server
	-lsof -ti:9001 | xargs kill 2>/dev/null
	./build/server$(SUFFIX)/bin/supergame-server

# --- Tests ----------------------------------------------------------------
# Both suites (shared/ simulation + server/ rosters & broadcast) in a headless
# build: no raylib, no client, and with BUILD_SERVER=OFF no uWebSockets
# either, so it configures and builds in seconds from a bare checkout.
# Same configuration CI runs. Configures every time on purpose.
test: tests
	ctest --preset tests

# The same suites under ASan + UBSan (the `tests-asan` preset, its own build
# dir). Seconds to run and no submodules, same as `test` - worth reaching for
# first on any "passes locally, fails in CI" test, which is exactly how L64's
# stack-use-after-scope in test_lobby.cpp was found. Also a CI job
# (test-asan-ubsan), so this is the local spelling of the same check.
test-asan:
	cmake --preset tests-asan
	cmake --build --preset tests-asan
	ctest --preset tests-asan

# Same suites against the full debug tree, for when it's already built and a
# second build dir isn't worth it.
test-debug:
	cmake --preset debug
	cmake --build --preset debug
	ctest --preset debug

# --- Housekeeping ---------------------------------------------------------

clean:
	rm -rf build

rebuild:
	rm -rf build/$(ALL_PRESET)
	$(MAKE) all BUILD=$(BUILD)
