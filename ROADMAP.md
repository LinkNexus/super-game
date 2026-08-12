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
| L20 | ✅ **Local 2-player co-op (same keyboard)** | New `Screen::SELECT_LOCAL_MODE` menu step to pick single vs. dual player before starting a `LocalSession`. P1 stays on arrows+space, P2 uses WASD (W to shoot). `Session::step`/`LocalSession`/`OnlineSession` widened from a single `PlayerInput` to `std::array<std::optional<PlayerInput>, MAX_PLAYERS>`; no `GameSim` changes needed since it already matched inputs to players by `player_id` rather than array slot (from the online work). Shook out a couple of bugs along the way worth remembering: an early version only emplaced `inputs_[0]` on the local-mode path, leaving the online path sending a `null` `PlayerInput` every tick (silently broken, no crash) — fixed by emplacing it once for both modes right after leaving the menu; also had to gate `getPlayersInputs()` to `Screen::PLAYING` only, since calling it while an input slot was still `std::nullopt` dereferenced an empty `std::optional` (UB). |
| L21 | ✅ **Scale enemy/boss difficulty to player count** | `GameSim::start()` now counts active player slots into `players_count_` and passes it into `enemies_pool_.init(playersCount)` and `boss_.init(playersCount)`. Enemies: renamed the old fixed `ROWS` to `MAX_ROWS`, added an `active_rows` lookup (`ROWS_PER_PLAYERS_COUNT`) so 1-player games spawn fewer active rows out of the same fixed `MAX_ROWS * COLS` array — no wire format change needed since dead slots just stay `alive = false`. Boss: `health = INITIAL_LP * playersCount`, and `spread_shot_bullets_count`/`successive_shots_bullets_count` are looked up per player count too. Also reworked `SPREAD_SHOT` from 3 hardcoded aimed bullets into a proper fan: computes the true angle between the boss's spawn point and the screen's bottom-left/bottom-right corners (`Vec2D::angle_between`, backed by a new `dot_product`), then distributes `spread_shot_bullets_count` bullets evenly across that angle via the existing (previously-unused) `Vec2D::rotated()` — covers the full screen width regardless of boss x-position, and bullet count scales with player count for free. **Bugs shaken out along the way**: (1) enemy row-culling condition was inverted (`idx / COLS < active_rows` killed the *first* N rows and kept the rest, instead of the other way around) — a 1-player game showed 2 rows instead of the intended 3; (2) the bullet-fan's `reserved_bullets` vector used `reserve()` (capacity only, `size()` stays 0) then wrote via `operator[]` — UB, and left `reserved_bullets.size() - 1` underflowing to a huge unsigned value that collapsed every bullet's angle offset to the same value (no actual fan) — fixed by sizing the vector up front with `std::vector<...>(spread_shot_bullets_count, nullptr)`; (3) `players_count_` had no default member initializer and was never reset at the top of `start()`, so it read garbage on a fresh `GameSim` and kept accumulating across repeated `start()` calls on the same instance — reachable in practice server-side, since a mid-match disconnect+reconnect reuses the same `Game`/`GameSim` object and re-triggers `start()` once the player slots refill — fixed by defaulting it to `{}` and explicitly zeroing it at the top of `start()` before the counting loop. |
| L22 | ✅ **Harden server against malformed client input** | `server/src/main.cpp`'s `.message` handler did `nlohmann::json::parse(message).get<shared::PlayerInput>()` with no try/catch — any malformed payload (bad JSON, missing/mistyped field) threw an uncaught `nlohmann::json` exception, crashing the whole process and dropping every active game, not just the offending connection. Fixed by wrapping the parse+deserialize in `try { ... } catch (const nlohmann::json::exception &) { return; }`, dropping the bad frame for that connection only. Client→server has no `{type, payload}` envelope/dispatch (unlike server→client's `MessageType` switch) — fine while `PlayerInput` is the only message going that direction, but the next client→server message type added will need the same enveloped-dispatch treatment. |
| L23 | ✅ **Fix restart button routing on Game Over/Win** | `KEY_R` on `GAME_OVER`/`WIN` now calls a dedicated `restart()` instead of duplicating `init()`'s full menu reset. Local: `startLocalSession(selected_local_mode_.value())` recreates the session with the mode the just-ended match used, straight into `Screen::PLAYING`. Online: `startOnlineSession()` reconnects straight to `Screen::CONNECTING`. `ENTER` still goes through `init()` back to `Screen::MENU`, unchanged. |
| L24 | ✅ **Player display names in online mode** | Name entered client-side on a new `Screen::NAME_ENTRY` (alnum + `_` only, to stay URL-safe), sent to the server as a `?name=` query param on the WS handshake and read server-side in a new `.upgrade` handler (the one uWebSockets callback that sees the raw `HttpRequest` before the socket exists). Threaded through `PlayerInfo` (`LobbyUpdate.players`) for the lobby list and `PlayerState.name` (patched onto `GameState` server-side after each `sim_.step()`, since `GameSim` itself stays identity-agnostic) for the in-game HUD label; local mode falls back to `"Player N"` since it has no server-assigned name. Picked up a matching L22-style hardening gap along the way: `OnlineSession::onMessage()` had no try/catch around its own deserialization — a client/server schema drift (or just a stale, not-yet-restarted server process) threw uncaught on IXWebSocket's background thread and killed the whole client; now caught and dropped the same way the server already handled malformed input. |
| L26 | ✅ **Lobby "ready" gate before starting a match** | Opportunistic addition to this branch, not originally scoped: online matches no longer auto-start the instant the second player connects — each player toggles ready via SPACE in the lobby (`ClientMessageType::READY` / `ReasyMessage`, the first real client→server envelope, sent alongside a now-enveloped `PlayerInput`), and `Game::tryStart()` only runs `sim_.start()` once `allPlayersReady()`. Redesigned `Screen::LOBBY` to list each connected player's name and ready state instead of just a join-count. Multiple bugs shaken out getting this end-to-end, all now fixed: `LobbyUpdate.game_started` was left on the old `playerCount == MAX_PLAYERS` check instead of the new ready gate, so both clients jumped to `PLAYING` the moment 2 connected while the server correctly still waited on `is_running_` — sim never started, screen just went blank; `setPlayerReady()` flipped state server-side but never re-sent `LobbyUpdate`, so a ready toggle was invisible to clients; `OnlineSession::step()`'s regular input send was never updated to the new `{type, payload}` envelope when `READY` introduced it, so every `PlayerInput` silently failed the server's type dispatch and got dropped — online movement/shooting did nothing. |
| L25 | **Doxygen documentation system** | Prof cares about this — worth doing properly, separate branch after the current one merges. Needs: a `Doxyfile` at the repo root, Doxygen-style doc comments (`\brief`/`@param`/etc.) added across the public API surface of `shared/`, `client/`, and `server/` (a deliberate one-off exception to this project's normal terse/no-comments style, since these are reference-doc comments, not inline WHY-comments), and a `.gitlab-ci.yml` job that runs `doxygen` and publishes the generated HTML (job artifact or GitLab Pages). |

### Lynce

| # | Task | Notes |
|---|------|-------|
| Ly11 | **Background music** | Loop a music track with `PlayMusicStream()`, fade in/out on menu vs gameplay. |
| Ly12 | **Screen shake on Boss hit** | Offset the camera/draw position by a small random amount for ~10 frames. Small effect, big polish. |
| Ly13 | **Menu screen** | Title "THE SUPER GAME", Play button, controls cheat sheet. |
| Ly14 | **Asset audit** | Make sure all assets load correctly on Windows and Mac (path separators, missing files). |

---

## Stretch ideas (post-deadline, not required for the module)

Things worth continuing after August 12 for fun, not part of the graded scope — no pressure to land these before the presentation.

| # | Idea | Notes |
|---|------|-------|
| S1 | **PvP mode (local and online)** | A real competitive mode alongside the current co-op — players fight each other instead of (or in addition to) enemies/boss. Would need: a mode-select alongside `LocalMode`/co-op online, player-vs-player collision/damage in `GameSim` (currently only `BulletType::ENEMY` bullets damage players — player bullets only hit enemies/boss), and a real win condition (last player standing) instead of the shared coop outcome. Big enough to warrant its own design pass, not a small task. |
| S2 | **Online leaderboard with accounts** | Persistent leaderboard across matches — needs accounts/auth, a backend datastore, way more infra than the rest of this project. Not happening before the module deadline; personal-interest continuation only. |

---

## Timeline at a glance

```
Week 1-2  (Jun 18 – Jul 1)   L1-L4  + Ly1-Ly3   — Core entities + visuals
Week 3-4  (Jul 1  – Jul 15)  L5-L8.5 + Ly4-Ly6  — Boss, states, HUD, effects, CI
Week 5-6  (Jul 15 – Aug 1)   L9-L15 + Ly7-Ly9   — Sim extraction + full networking
Week 7    (Aug 1  – Aug 5)   L16    + Ly10        — Docker + polish screens
Week 8    (Aug 5  – Aug 12)  L17-L19 + Ly11-Ly14 — Deploy, final polish, demo
```
