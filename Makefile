.PHONY: all debug release configure configure-debug configure-release \
        run run-debug run-release clean rebuild

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
	./build/debug/bin/supergame-client

run-client-release: release
	./build/release/bin/supergame-client

clean:
	cmake --build --preset debug --target clean

rebuild:
	rm -rf build/debug
	cmake --preset debug
	cmake --build --preset debug
