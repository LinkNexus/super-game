# Headless build of supergame-server only. BUILD_CLIENT=OFF skips the raylib
# client entirely, so this needs none of raylib's GUI/graphics system deps
# (X11, OpenGL, ALSA...) - just a C++20 toolchain.
#
# Build context must already have submodules populated the same way the
# README describes (selective, not --recursive):
#   git submodule update --init vendor/uWebSockets
#   git -C vendor/uWebSockets submodule update --init uSockets
# vendor/raylib and vendor/ixwebsocket don't need to be populated at all for
# this image - they're excluded via .dockerignore and never referenced when
# BUILD_CLIENT=OFF.

FROM debian:trixie-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake \
    ninja-build \
    g++ \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_CLIENT=OFF \
    && cmake --build build --target supergame-server -j"$(nproc)"

FROM debian:trixie-slim AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --no-create-home supergame

COPY --from=builder /app/build/bin/supergame-server /usr/local/bin/supergame-server

USER supergame
EXPOSE 9001

ENTRYPOINT ["/usr/local/bin/supergame-server"]
