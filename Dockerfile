# Headless build of supergame-server only. BUILD_CLIENT=OFF skips the raylib
# client entirely, so this needs none of raylib's GUI/graphics system deps
# (X11, OpenGL, ALSA...) - just a C++20 toolchain. BUILD_TESTS=OFF likewise
# skips the unit-test target: tests run in CI, not in the shipped image.
#
# Build context must already have submodules populated the same way the
# README describes (selective, not --recursive):
#   git submodule update --init vendor/uWebSockets
#   git -C vendor/uWebSockets submodule update --init uSockets
# vendor/raylib and vendor/ixwebsocket don't need to be populated at all for
# this image - they're excluded via .dockerignore and never referenced when
# BUILD_CLIENT=OFF.

# Single place to pin the base image for both stages. `trixie-slim` is a
# rolling tag, so a rebuild months apart is not reproducible; override this
# with a digest (--build-arg DEBIAN_IMAGE=debian@sha256:...) to pin exactly.
ARG DEBIAN_IMAGE=debian:trixie-slim

FROM ${DEBIAN_IMAGE} AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy the vendored dependency trees before the application sources. vendor/
# is by far the largest and least frequently touched part of the context, so
# giving it its own layer means an edit under shared/ or server/ invalidates
# only the two layers below it, not the vendor upload.
#
# Configure and build stay in a single RUN: CMake resolves the source lists in
# shared/CMakeLists.txt and server/CMakeLists.txt at configure time and errors
# out on any file that isn't present yet, so there is no useful
# configure-before-sources split to make here.
COPY vendor ./vendor
COPY CMakeLists.txt CMakePresets.json ./
COPY cmake ./cmake
COPY shared ./shared
COPY server ./server

RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CLIENT=OFF \
    -DBUILD_SERVER=ON \
    -DBUILD_TESTS=OFF \
    && cmake --build build --target supergame-server -j"$(nproc)"

FROM ${DEBIAN_IMAGE} AS runtime

# curl is here for the HEALTHCHECK below, not for the server itself - without
# an in-image HTTP client, Docker/Dokploy can only see "process alive", which
# a wedged event loop would still satisfy.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    curl \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --no-create-home supergame

COPY --from=builder /app/build/bin/supergame-server /usr/local/bin/supergame-server

USER supergame
EXPOSE 9001

# Hits the server's own /health route, so this fails if the uWebSockets event
# loop stops serving even while the process is still up.
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -fsS http://127.0.0.1:9001/health || exit 1

ENTRYPOINT ["/usr/local/bin/supergame-server"]
