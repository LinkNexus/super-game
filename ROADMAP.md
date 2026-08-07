# Project Roadmap — The Super Game

**Deadline:** August 12 | **Start:** June 18 | **~8 weeks total**

The overall approach: local game first, then server, then multiplayer.

---

## Phase 1 — Local Game (now → ~July 15)

**Goal:** A complete, playable single-machine game with enemies, shooting, a boss, and collision.

### Levy

| # | Task | Notes |
|---|------|-------|
| L1 | **Refactor `main.cpp` into proper files** | Split into `Game`, `Player`, `Bullet`, `Enemy`, `Boss` classes/structs in separate `.h/.cpp` files under `client/src/`. Fix the y-boundary bug on line 31 too. |
| L2 | **Bullet/projectile system** | Pool of bullets, fired on SPACE, move upward at fixed speed, destroyed off-screen. |
| L3 | **Enemy grid system** | 4–5 rows × 10 columns, two enemy types. Enemies move left/right and step down (Space Invaders pattern). Random enemy shoots down. |
| L4 | **AABB collision detection** | Bullet↔Enemy, Bullet↔Player (enemy bullets), Player↔Enemy. Must be reusable for Phase 2. |
| L5 | **Boss enemy** | Appears after clearing enemies. Has a health pool (e.g. 20 hits), shoots in patterns, moves horizontally. |
| L6 | **Game state machine** | States: `MENU → PLAYING → GAME_OVER / WIN`. Clean transitions, pause support. |
| L7 | **Score & player health system** | Points per kill, player has 3 lives. Reset on new game. |
| L8 | **`shared/` message structs skeleton** | Define `PlayerInput` and `GameState` structs in `shared/` (needed in Phase 2). No networking yet — just the data layout. |
| L8.5 | ✅ **GitLab CI/CD pipeline** | `.gitlab-ci.yml` builds client (debug+release, Linux), a cross-compiled Windows client release (see L16), and server (debug+release) via CMake presets on every push, plus caches each build dir per branch. Uses `GIT_SUBMODULE_STRATEGY: normal` + a manual `uSockets` init step, **not** `recursive` — the original plan turned out to be wrong: `--recursive` pulls in uWebSockets'/uSockets' own submodules (BoringSSL, lsquic, a fuzz corpus, 1GB+) that this project doesn't need, per README.md. Extended in Phase 2 with the Docker image build (see L16); unit tests still pending, no test framework wired up yet. |

### Lynce

| # | Task | Notes |
|---|------|-------|
| Ly1 | **Starfield background** | Render 100–200 white dots at varying speeds/sizes for a parallax feel. A simple `Star` struct array updated each frame — good first C++ exercise. |
| Ly2 | **Player ship sprite** | Load a texture with `LoadTexture()`, replace the triangle. Learn `DrawTextureEx` for rotation/scaling. |
| Ly3 | **Enemy sprites** (2 types + Boss) | Spritesheet or individual PNGs. Animate using frame counter (e.g. toggle frame every 30 ticks). |
| Ly4 | **HUD — score & lives display** | Draw score top-left, draw heart icons for lives. Use `DrawText`, `DrawTexture`. |
| Ly5 | **Boss health bar** | A red/gray rectangle at the top that shrinks as boss takes hits. Drawn only when boss is active. |
| Ly6 | **Explosion effect** | When enemy is destroyed: spawn 8–12 particles (colored rectangles/circles) that fly outward and fade. A `Particle` struct with position, velocity, lifetime — real C++ struct/array work. |

---

## Phase 2 — Networking (July 15 → ~August 5)

**Goal:** Two players on separate machines, server runs authoritative simulation. The client keeps a **local mode** (offline single-player) alongside the new online mode — one binary, mode chosen in the menu, both modes running the same shared simulation.

### Levy

| # | Task | Notes |
|---|------|-------|
| L9 | **Add `server/` CMake target** | New `add_subdirectory(server)` in root `CMakeLists.txt`, headless binary (no Raylib). |
| L10 | **Integrate µWebSockets into server** | Add as git submodule under `vendor/`, wire into server's `CMakeLists.txt`. |
| L11 | ✅ **Shared authoritative game loop (`GameSim`)** | Entities currently mix sim data and render data (e.g. `Player` holds `position`/`lives` *and* `Texture2D`/`draw()`), and their update logic calls raylib directly (`IsKeyDown`, etc.). Migrate one entity at a time (`Bullet` → `Enemy` → `Player` → `Boss`, simplest first): move its state into a plain `SimState` struct in `shared/` and its update logic into `shared/` alongside it (consuming `PlayerInput`, no raylib), leaving only `Texture2D` + `draw()` on the client side. State and logic move together per entity — they can't be split into separate passes without duplicating state. End state: a headless `GameSim` (world state + `step(inputs, dt)`) that the server runs authoritatively at fixed 60 Hz, and that the client's local mode runs in-process — no duplicated logic. Headless sim = unit-testable → wire tests into the L8.5 CI pipeline. |
| L12 | ✅ **Network protocol** | Define the wire format: Client sends `PlayerInput` (keys bitmask + timestamp). Server broadcasts `GameState` (all positions, health, score) to all clients. Start with JSON, optimise to binary if needed. |
| L13 | ✅ **Client↔server session wiring** | Was split into three separate tasks (WS integration, local/online mode selection, lobby/join) — turned out to be one coupled unit: applying a received state snapshot has no home without the session split, and neither is testable without a real two-player session on the server. Done as one milestone: (1) **server** — `GameManager` wires `.open` to create/join a session via `addPlayer`, capped at 2 players, `.close` cleans up; (2) **client** — session layer (`LocalSession` vs `OnlineSession`) plus a menu entry to pick between them; (3) real WS send/receive wired into `OnlineSession`, with Player 1/2 assigned by join order via a server `WELCOME` message. 3rd+ connections auto-matchmake into a new independent game rather than being rejected outright — a deliberate deviation from the original wording, arguably the better behavior. |
| L15 | ✅ **Client-side interpolation** | Smooth rendering between received state snapshots to hide network jitter (lerp positions). `OnlineSession` lerps players/bullets/boss/enemy-offset between `previous_state_` and `target_state_` based on elapsed time vs. `shared::FIXED_DT`. |
| L16 | ✅ **Docker setup** | Multi-stage `Dockerfile` (root) builds just `supergame-server` via a new `BUILD_CLIENT` CMake option that skips raylib/ixwebsocket entirely — the headless server image needs none of raylib's X11/OpenGL/ALSA deps. `.gitlab-ci.yml`'s `docker:server` job pushes it to this project's GitLab Container Registry via Kaniko (default branch only). Not yet tested with a local `docker run` — Docker Desktop wasn't running when this was built; worth a manual smoke test. **Also added**: a Windows client build via MinGW-w64 cross-compilation (`cmake/mingw-w64-toolchain.cmake` + `windows-client-release` preset) for the prof's Windows-only grading setup — runs on the same Linux runner, no Windows/macOS runner needed. Statically links the MinGW runtime so the `.exe` needs no extra DLLs on a stock Windows machine, and builds as a GUI (not console) subsystem app. Verified locally via Homebrew's mingw-w64: configures, builds, and produces a valid standalone PE32+ GUI executable with only stock Windows DLL dependencies — but never actually run on real Windows (no Wine available to test rendering/input). `build:client:windows` in CI uploads it as a job artifact. True macOS cross-compilation isn't feasible from this Linux runner (no legitimate way to get Apple's SDK there) — a macOS build would have to happen natively on an actual Mac. |

### Lynce

| # | Task | Notes |
|---|------|-------|
| Ly7 | **Player 2 visual** | Second ship sprite/color, mirrored or distinct from Player 1. |
| Ly8 | **Connection UI** | A simple "Connecting…" / "Waiting for player 2…" screen shown before the game starts. Draw text + a blinking dot animation. |
| Ly9 | **Sound integration** | `InitAudioDevice()`, load `.wav`/`.ogg` files for: shoot, explosion, boss hit, game over. Hook into game events. Implement a simple `AudioManager` (load once, play on demand). |
| Ly10 | **Score screen / Game Over screen** | Show final score, who won, "Press R to restart". Nice visual layout with text and maybe a simple animated border. |

---

## Phase 3 — Polish (August 5 → August 12)

**Goal:** Presentable, demo-ready, no loose ends.

### Levy

| # | Task | Notes |
|---|------|-------|
| L17 | ✅ **Deploy server to VPS via Dokploy** | Live at `supergame.levynkeneng.dev` (Cloudflare → Nginx Proxy Manager → Dokploy's Traefik → container). Chain of issues hit and fixed along the way: `.listen(9001, ...)` preferred IPv6 and silently failed to bind in this IPv6-disabled environment (fixed: `.listen("0.0.0.0", 9001, ...)`); stdout was fully block-buffered in the container so that failure was invisible in `docker logs` (fixed: `setvbuf(stdout, nullptr, _IONBF, 0)` + `fflush`); NPM needed its "Websockets Support" toggle enabled for the WS upgrade to pass through. **Known recurring issue**: Docker Swarm's default `vip` endpoint mode is broken on this host — the service name resolves to a VIP that gets "connection refused" even though the container itself is healthy and reachable directly by its task IP. Fix: `docker service update --endpoint-mode dnsrr <current-service-name>` (get the current name via `docker ps \| grep super` — Dokploy generates a new random suffix on every redeploy). **This does not persist** — Dokploy regenerates the service on every redeploy and resets it back to `vip`, so this command needs re-running after each one. Root cause: the VPS is a Proxmox LXC, where Dokploy's own Proxmox auto-detection sets `dnsrr` for its *own* core services (Traefik/Postgres/Dokploy itself) but not for apps deployed afterward through the dashboard — those always come up as Swarm services in `vip` mode regardless. **Actual permanent fix (not yet done)**: redeploy as a Dokploy **Compose**-type app in **"Docker Compose" mode** (plain `docker compose up`, genuinely not Swarm) instead of the current **Application**-type app (always Swarm). Compose mode doesn't support the `build:` directive, but that's not needed here since the image is already built via CI and pushed to Docker Hub — just `image: levynkenenghtw/supergame-server:latest` in a small `docker-compose.yml`, plus Dokploy's Domains tab auto-injects the Traefik labels. This is exactly what the Laravel/Compose-deployed portfolio app already does successfully with no VIP issues. Levy owns the server/deployment side solo; Lynce isn't involved in this part. |
| L18 | **Stress test & fix desyncs** | Run two clients, simulate bad network (packet delay), fix any state drift or crash. |
| L19 | **Enemy bullet system on server** | Make sure enemy shooting is server-authoritative (not client-predicted). |

### Lynce

| # | Task | Notes |
|---|------|-------|
| Ly11 | **Background music** | Loop a music track with `PlayMusicStream()`, fade in/out on menu vs gameplay. |
| Ly12 | **Screen shake on Boss hit** | Offset the camera/draw position by a small random amount for ~10 frames. Small effect, big polish. |
| Ly13 | **Menu screen** | Title "THE SUPER GAME", Play button, controls cheat sheet. |
| Ly14 | **Asset audit** | Make sure all assets load correctly on Windows and Mac (path separators, missing files). |

---

## Timeline at a glance

```
Week 1-2  (Jun 18 – Jul 1)   L1-L4  + Ly1-Ly3   — Core entities + visuals
Week 3-4  (Jul 1  – Jul 15)  L5-L8.5 + Ly4-Ly6  — Boss, states, HUD, effects, CI
Week 5-6  (Jul 15 – Aug 1)   L9-L15 + Ly7-Ly9   — Sim extraction + full networking
Week 7    (Aug 1  – Aug 5)   L16    + Ly10        — Docker + polish screens
Week 8    (Aug 5  – Aug 12)  L17-L19 + Ly11-Ly14 — Deploy, final polish, demo
```
