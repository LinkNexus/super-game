.PHONY: all debug release configure configure-debug configure-release \
        run run-debug run-release clean rebuild test test-debug

# Default target
all: debug

configure: configure-debug

configure-debug:
	cmake --preset debug

configure-release:
	cmake --preset release

debug:
	cmake --build --preset debug

release:
	cmake --build --preset release

run-client: run-client-debug

run-client-debug: debug
	./build/debug/bin/supergame-client ws://localhost:9001

run-server: run-server-debug

run-server-debug: debug
	-lsof -ti:9001 | xargs kill 2>/dev/null
	./build/debug/bin/supergame-server

run-client-release: release
	./build/release/bin/supergame-client ws://localhost:9001

run-server-release: release
	./build/release/bin/supergame-server

run-debug: run-server-debug run-client-debug

run-release: run-server-release run-client-release

run: run-debug

# Both suites (shared/ simulation + server/ rosters & broadcast) in a headless
# build: no raylib, no client, so it configures and builds in seconds and needs
# none of the graphics deps. Same configuration CI runs. Configures every time
# on purpose, so this works from a fresh checkout with no setup step.
test:
	cmake --preset tests
	cmake --build --preset tests
	ctest --preset tests

# Same suites against the full debug tree, for when it's already built and a
# second build dir isn't worth it.
test-debug: debug
	ctest --preset debug

clean:
	cmake --build --preset debug --target clean

rebuild:
	rm -rf build/debug
	cmake --preset debug
	cmake --build --preset debug
