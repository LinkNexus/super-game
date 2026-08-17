# Project Roadmap — The Super Game

**Start:** June 18 | **Course deadline:** August 12 (met — graded, passed)

Phases 1-3 below are the graded module and are done, split between two contributors as
recorded in each phase's task tables. From Phase 4 onward, Levy is the sole contributor
(Lynce is no longer on the project) — all tasks, gameplay and visual/audio/UI alike, are his.

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

## Phase 4 — Game modes: Coop (1-4 players) & PvP (1v1 / 2v2) (no deadline)

**Goal:** An explicit mode choice, both local and online, instead of one fixed coop-vs-boss
game:

- **Local:** Coop (1-2 players, already shipped as L20) *or* PvP 1v1 (same machine,
  arrows vs. WASD, players fight each other instead of the boss).
- **Online:** Coop (**variable** headcount, 1-4 players — however many connect, no forced
  even/odd pairing, difficulty scales to whatever the actual count is) *or* PvP, chosen as
  either 1v1 or 2v2. A 2v2 team can be one machine bringing 2 local players (reusing L20's
  co-op input handling for a single connection), or two separate solo connections grouped
  together.

Coop and PvP are two genuinely different rule sets sharing the same sim/session plumbing —
coop keeps today's boss-fight model (adapted to any count 1-4), PvP replaces it with
player-vs-player damage and a last-player/last-team-standing win condition. Both need to stay
selectable, not one replacing the other.

Today `shared::MAX_PLAYERS = 2` is already the single source of truth threaded through the
wire structs (`GameState.players`, `LobbyUpdate.players`) and `GameSim`'s player array, so
raising it to 4 is mostly mechanical. The real gaps: (1) a `PlayerConnection` on the server
currently maps 1:1 to exactly one player slot (`Game::addPlayer` takes a single connection) —
nothing today lets one WS connection claim two slots the way one local keyboard already
drives two players in `LocalSession`; (2) `GameSim` has no notion of "mode" at all yet —
collision is unconditionally coop rules (`BulletType::ENEMY` bullets damage players, player
bullets only ever hit enemies/boss); and (3) the L21 difficulty tables
(`ROWS_PER_PLAYERS_COUNT`, boss health/bullet counts) only have entries for 1-2 players, not
the 3 a variable-count coop game now needs too.

All tasks below are Levy's — no gameplay/visual split anymore.

| # | Task | Notes |
|---|------|-------|
| L27 | ✅ **Mode + size selection UI** | `Screen::SELECT_LOCAL_MODE` (L20) extended from a single/dual toggle to a 3-way `local_modes_order_` cycle (Single/Dual/PvP); new `Screen::SELECT_ONLINE_MODE` step does the same for Coop/1v1/2v2 via `online_modes_order` before `NAME_ENTRY`, with the chosen size sent to the server as `&players=N` on the WS handshake URL (consumed once L29 lands). `Game::draw()`'s single switch-statement body was split into one `draw*()` helper per screen (`drawMainMenu`, `drawLocalModeSelection`, `drawOnlineModeSelection`, `drawGame`, `drawEndScreen`, etc.) to keep the growing screen count readable. Actual PvP collision/win-condition behavior is out of scope here — that's L28; picking PvP today still plays out as a coop match rules-wise. Bug caught in review before merging: the new PvP option didn't route through the existing second-player wiring (`ids[1]`/`inputs_[1]`/WASD polling), which was gated to `mode_ == LocalMode::DUAL_PLAYER` only — selecting PvP silently played as single-player. Fixed by widening those checks to `!= LocalMode::SINGLE_PLAYER` in `session.cpp`, `game.cpp::startLocalSession`, and `game.cpp::getPlayersInputs`. Also added (untracked, unwired prep for L28): `shared/modes.h`/`modes.cpp` with a `CoopMode`/`PvPMode`/`Mode` variant, included by `game_sim.h` but not yet used by `GameSim`. |
| L28 | **`GameSim` mode flag governing collision + win condition** | A `Coop`/`PvP` flag on `GameSim` (or per-match config) gates: whether player bullets can damage other players (off in coop, on in PvP, never same-team in 2v2), and which win-condition check runs — boss-defeated (coop, unchanged) vs. last-player-standing (PvP 1v1) vs. last-team-standing (PvP 2v2). Built on top of the existing AABB collision system (L4), not a replacement for it. |
| L29 | **Slot-claim handshake** | Extend the existing `?name=` WS handshake query param (see L24) with something like `?players=1\|2` so a connection declares up front how many local slots it wants, plus a name per claimed slot. Needed both for an online coop group bringing 2 people on one machine, and for a 2v2 PvP team formed the same way. Server-side `GameManager`/`Game::addPlayer` needs to reserve N slots atomically for one connection instead of always 1. |
| L30 | **Variable-count coop difficulty scaling (1-4)** | Extend `ROWS_PER_PLAYERS_COUNT` (`enemy_sim.h`) and the boss spread/successive-shot tables (`boss_sim.h`) from their current 1-2-player entries (L21) to cover 3 and 4 too — coop accepts whatever headcount shows up, odd or even, no pairing requirement. |
| L31 | **Team assignment for PvP 2v2** | How two duos/solos pair into Team A vs. Team B — a lobby-side "join team" pick, or auto-balance on connect (first arrival(s) to fill Team A, rest to Team B). |
| L32 | **Multi-slot `PlayerInput` per tick** | One connection can now speak for up to 2 players each tick (coop group or 2v2 team) — `OnlineSession`'s send path and the server's message handler both currently assume one `PlayerInput` per message per connection (per the L22 hardening note on the missing envelope/dispatch). Needs a `player_id` per input so the server routes each one to the right `GameSim` slot. |
| L33 | **Lobby UX: mode/size-aware matchmaking + ready gating** | `Screen::LOBBY` (L26) currently assumes one undifferentiated 2-player queue. Needs separate pools per (mode, size) so a coop seeker never lands in a PvP lobby and vice versa, a team-select affordance for 2v2 (from L31), and per-connection ready toggling so a duo readies up once, not twice per slot. |
| L34 | **Mode-aware player visuals** *(was Ly15, Lynce's territory)* | Two visual schemes depending on mode: coop keeps/extends the existing per-player distinction (Ly7) up to 4 players; PvP needs team-colored ships instead (Team A one color family, Team B another, same-machine duo still distinguishable within their team's palette) plus a HUD that splits into two score/lives clusters in 2v2 rather than one shared readout. |

---

## Phase 5 — Web Client (WASM) (no deadline)

**Goal:** Play the game in a browser. Not a WASM port of the raylib `client/` binary itself —
raylib's own Emscripten support is fine for rendering, but the client's networking
(`ixwebsocket`) doesn't map onto a browser sandbox, and its blocking `Game::run()` loop needs
restructuring around `emscripten_set_main_loop` regardless. Instead: a new `web/` module, a
sibling of `client/`/`server/`/`shared/`, with two pieces —

- `web/wasm/` — a thin C++ binding layer over `shared/` (already zero-dependency: no threads,
  no filesystem, no networking, just header-only `nlohmann_json` — a clean Emscripten target
  as-is, no changes needed to `shared/` itself) compiled via `emcmake`/Emscripten into a
  `.wasm` + glue `.js`. Used for **local mode only** — this is where `GameSim` actually runs
  in-browser.
- `web/frontend/` — a TypeScript app (Canvas2D rendering, keyboard input), mirroring the
  desktop client's input/render split (`Game::getPlayerInputs()`/`handleInput()` vs. `draw()`).

**Key design decision:** the WASM boundary reuses the *existing* wire format instead of
inventing a new one. `PlayerInput`/`GameState` already have `to_json`/`from_json` via
`nlohmann_json` (`shared/include/shared/messages.h`) for the real network protocol — the WASM
binding shim can serialize/deserialize through those exact same functions, passing JSON
strings across the boundary. That means the TS side gets one `draw(gameState)` function that
doesn't care whether the JSON came from a local WASM `step()` call or a real server message —
the same "draw() is source-agnostic" principle `client/` already follows (see
[CLAUDE.md](CLAUDE.md)), extended to a second frontend. Online mode on web skips WASM
entirely: it's a native browser `WebSocket` speaking the same wire protocol directly to
`supergame-server`, with a TS port of `OnlineSession`'s interpolation lerp (the one piece of
client logic that isn't in `shared/` and so can't be reused as-is).

All tasks below are Levy's.

| # | Task | Notes |
|---|------|-------|
| L35 | **Emscripten toolchain + CMake wiring** | New `cmake/emscripten-toolchain.cmake` + CMake preset (mirrors the existing `windows-client-release`/`cmake/mingw-w64-toolchain.cmake` cross-compile pattern), new `web/wasm/CMakeLists.txt` producing `supergame-web-sim` — links `supergame-shared` unchanged, gated behind a `BUILD_WEB` option so normal desktop/server builds are unaffected. |
| L36 | **WASM binding shim (JSON in/out over the wire format)** | Thin C++ layer exposing `GameSim::start()`/`step()` to JS (via Embind), taking a `PlayerInput` JSON string in and returning a `GameState` JSON string out — reusing `messages.h`'s existing `to_json`/`from_json`, no second marshaling scheme invented. |
| L37 | **`web/frontend/` scaffold** | TS project (Vite or similar) with Canvas2D rendering and keyboard→`PlayerInput` JSON input handling, structured to mirror the input/render split already established in `client/`. |
| L38 | **Unified `draw(gameState)` on the TS side** | One render function for both local (WASM `step()` output) and online (server message) `GameState` JSON — same source-agnostic principle as the desktop client's `draw()`. |
| L39 | **Online mode via native browser `WebSocket`** | TS client speaking the existing wire protocol directly to `supergame-server` (same `?name=`/`?players=` handshake as desktop, once L28/L29 land), plus a TS port of `OnlineSession`'s client-side interpolation lerp. |
| L40 | **Web asset pipeline** | Lazy-load audio behind a user-gesture "Play" button (browsers block autoplay pre-interaction anyway, so this is required, not optional), and re-encode `Backgroundsound.wav` (48MB) to a compressed web-friendly format — bundling the raw wav would make first load brutal. |
| L41 | **CI: build + deploy the web target** | New GitHub Actions job building the `web-sim` WASM preset + frontend bundle, mirroring the existing cross-compile job pattern — artifact upload/deploy gated to master pushes only, consistent with the rest of the pipeline's storage-conscious triggers. |

---

## Phase 6 — 3D Migration: On-Rails Corridor Shooter (no deadline)

**Goal:** Real 3D gameplay, not just a 3D presentation of the current 2D game — a Star
Fox-style on-rails corridor shooter, third-person camera trailing behind the player's ship.
The world/enemy formations advance along a fixed Z axis toward the player; the player doesn't
fly freely, but steers within a bounded 2D plane (X/Y) a fixed distance in front of the
camera — 2 degrees of freedom, vs. today's 1 (`PlayerSimState::POSITION_Y` is currently a
fixed constant; only X varies). This is a real redesign of `shared/`'s spatial model, not a
rendering swap: `Vec2D` positions become `Vec3D` throughout the sim, and collision, movement,
and the boss's already-angle-based spread-shot fan (`Vec2D::rotated`/`angle_between` — L21)
all need a 3D equivalent.

**Sequencing note:** this touches nearly every file Phase 4 (PvP collision) and Phase 5
(WASM/wire format) also touch. Doing the dimensionality migration first — or at least before
deep investment in Phase 4's collision work — avoids writing 2D collision logic that gets
thrown away almost immediately after. Phase 5's core design (the WASM shim reusing
`to_json`/`from_json`) is unaffected either way — it's format-agnostic; 3D coordinates are
just more fields in the same JSON.

**Open question worth resolving before starting:** does 3D fully replace the 2D game, or
become an additional selectable mode alongside it (extending Phase 4's mode-select step with
a dimensionality axis, not just Coop/PvP)? Affects whether `Vec2D` and the current 2D
collision/rendering path get deleted outright or kept around for a 2D mode.

All tasks below are Levy's.

| # | Task | Notes |
|---|------|-------|
| L42 | **`Vec3D` primitives in `shared/`** | Add a 3D vector type alongside the existing `Vec2D` (`math_utils.h`), generalizing the angle/rotation math already built for the boss's spread-shot fan (`Vec2D::rotated`, `angle_between`, `dot_product` — L21) to 3D (e.g. spreading a bullet volley across a cone instead of a 2D arc). |
| L43 | **Migrate sim positions `Vec2D` → `Vec3D`** | `PlayerSimState`, `BulletSimState`, `EnemySimState`, `BossSimState` (and their wire counterparts in `messages.h`) move to 3D positions, one entity at a time — same "state and logic migrate together" principle already established for the client/server split (see [CLAUDE.md](CLAUDE.md)), applied to a dimensionality change instead. |
| L44 | **Player steering: 2-DOF movement plane** | Today `PlayerSimState::POSITION_Y` is fixed — the player only ever moves in X. On-rails 3D needs real X/Y steering within a bounded plane a fixed distance in front of the camera (Z stays constant for the player; only the *world* advances in Z). Needs a new `UP`/`DOWN` bit on `PlayerInput.buttons` alongside the existing left/right/fire bits. |
| L45 | **World/enemy Z-advance ("the rail")** | Reinterpret the existing enemy row/column formation (`enemy_sim.h`) as an (X,Y) formation plane spawned at a far Z that shrinks toward the player over time, replacing the current "step down the 2D grid" logic — keeps the existing grid/wave structure, just adds depth instead of a full rewrite. |
| L46 | **3D collision (AABB → 3D bounding box)** | Generalize the hitbox constants (currently a single `SIZE` per entity, e.g. `PlayerSimState::SIZE`) to width/height/depth, and extend the AABB collision check used throughout `GameSim` to 3 axes. |
| L47 | **Boss spread-shot fan in 3D** | The existing angle-fan bullet pattern (L21) generalizes from a 2D arc to a 3D cone using the new `Vec3D` rotation math from L42. |
| L48 | **Third-person 3D camera + rendering** | Client-side: a `Camera3D` trailing behind/above the player's ship, replacing 2D `DrawTexture` calls with billboards or simple 3D models. Biggest raylib-API learning curve of this phase — raylib's 3D camera/mesh/lighting surface is quite different from the 2D drawing calls used everywhere in `client/` today, which is exactly the point per the stated goal of gaining 3D experience. |

---

## Phase 7 — Online Leaderboard with Accounts (no deadline)

**Goal:** Persistent accounts and a real leaderboard across matches. No longer a stretch idea
now that there's no deadline pressure — a genuine phase.

Decided: this lives in a **separate backend service**, not bolted onto `supergame-server`.
The game server's whole design is a fixed-tick authoritative sim loop over WebSockets; a
stateless CRUD/auth API is a different concern and shouldn't leak into that process. It
deploys alongside `supergame-server` on the same VPS, reusing the Dokploy/Compose deployment
pattern already established in L17. Auth is rolled by hand (hashed password + JWT/session),
not delegated to a third-party provider — deliberate, since writing it is part of the point.

Two things left genuinely open, worth deciding at the start of this phase rather than now:
**language/framework** for the new service (doesn't have to be C++ — this is a CRUD+auth API,
a different shape of problem than the sim), and **database** (a lightweight embedded DB like
SQLite vs. a real Postgres instance — worth checking first whether the VPS's existing Dokploy
deployment already runs a Postgres instance for its own core services, per the L17 notes, that
could be reused instead of standing up a new one).

This has a soft dependency on Phase 4: leaderboard entries are more useful once they can be
filtered by mode (coop/PvP, 1v1/2v2), but that's an additive column, not a blocker — score
tracking can start against raw matches before Phase 4 lands.

All tasks below are Levy's.

| # | Task | Notes |
|---|------|-------|
| L49 | **New backend service scaffold** | A new top-level directory (e.g. `leaderboard-service/`), separate from `client`/`server`/`shared` since it doesn't touch the sim at all. Language/framework choice is open — see above. |
| L50 | **Accounts: schema + password auth** | User table (id, username, hashed password via bcrypt/argon2), registration/login endpoints, JWT (or session cookie) issuance and validation. |
| L51 | **Leaderboard: schema + score submission** | Persistent scores table (user_id, score, mode, timestamp), authenticated endpoint for the game client to submit a completed match's score tied to the logged-in account. |
| L52 | **Client-side account UI** | New `Screen` states for login/register, storing the issued auth token client-side, and submitting the score on `GAME_OVER`/`WIN` when logged in. |
| L53 | **Leaderboard display screen** | A new screen listing top scores (optionally filtered by mode once Phase 4 lands), fetched from the backend service's API. |
| L54 | **Deployment** | New Dokploy/Compose service alongside `supergame-server` on the VPS, reusing the L17 deployment pattern; resolve the DB choice (see above) as part of this task. |
| L55 | **CI: build/test the backend service** | New GitHub Actions job for the new service, gated the same storage-conscious way as the rest of the pipeline (master pushes + PRs, no artifact bloat on every branch push). |

---

## Timeline at a glance

Historical record of the graded module's schedule (Phases 1-3). Nothing from Phase 4 onward
is scheduled — no deadline, no fixed week-by-week plan.

```
Week 1-2  (Jun 18 – Jul 1)   L1-L4  + Ly1-Ly3   — Core entities + visuals
Week 3-4  (Jul 1  – Jul 15)  L5-L8.5 + Ly4-Ly6  — Boss, states, HUD, effects, CI
Week 5-6  (Jul 15 – Aug 1)   L9-L15 + Ly7-Ly9   — Sim extraction + full networking
Week 7    (Aug 1  – Aug 5)   L16    + Ly10        — Docker + polish screens
Week 8    (Aug 5  – Aug 12)  L17-L19 + Ly11-Ly14 — Deploy, final polish, demo
```
