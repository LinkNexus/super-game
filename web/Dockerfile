# The web client (L41): the Release wasm sim + the Vite bundle, served by
# Caddy, which also fronts supergame-server (see web/Caddyfile). Build from the
# repo root:
#
#   docker build -f web/Dockerfile .
#
# Context needs only the vendor/mathlib submodule. What gets sent is trimmed by
# web/Dockerfile.dockerignore (BuildKit's per-Dockerfile ignore file), not the
# server's .dockerignore.

# Same emscripten as flake.nix's `web` shell and CI's test-web job - bump
# together. The image ships node too, so one stage builds wasm and bundle.
ARG EMSDK_IMAGE=emscripten/emsdk:6.0.9
ARG CADDY_IMAGE=caddy:2-alpine

FROM ${EMSDK_IMAGE} AS builder

RUN apt-get update && apt-get install -y --no-install-recommends ninja-build \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# npm deps first: package-lock.json changes far less often than the sources.
COPY web/frontend/package.json web/frontend/package-lock.json ./web/frontend/
RUN cd web/frontend && npm ci --no-fund --no-audit

COPY vendor ./vendor
COPY CMakeLists.txt CMakePresets.json ./
COPY cmake ./cmake
COPY shared ./shared
COPY web/wasm ./web/wasm

# -B build/web: the frontend imports the module from build/web/bin (the @wasm
# alias in vite.config.ts and tsconfig.json), whichever preset produced it.
RUN cmake --preset web-release -B build/web \
    && cmake --build build/web --target supergame-web-sim

COPY assets ./assets
COPY web/frontend ./web/frontend

# Baked in at build time: Vite inlines VITE_* into the bundle. An env var
# beats web/frontend/.env*.
ARG GAME_SERVER_URL=wss://supergame.levynkeneng.dev/ws
RUN cd web/frontend && VITE_GAME_SERVER_URL=${GAME_SERVER_URL} npm run build

FROM ${CADDY_IMAGE} AS runtime

COPY web/Caddyfile /etc/caddy/Caddyfile
COPY --from=builder /app/web/frontend/dist /srv

# Served at GET /web-version, so CI can tell the redeployed container from the
# old one (the server's /version does the same). Last, so a new SHA only
# changes this layer.
ARG GIT_SHA=unknown
ENV SUPERGAME_WEB_VERSION=${GIT_SHA}

EXPOSE 80

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
    CMD wget -qO- http://127.0.0.1/web-version || exit 1
