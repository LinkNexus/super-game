# Project Roadmap — The Super Game

**Start:** June 18 | **Course deadline:** August 12 (met — graded, passed) | Sole contributor: Levy

A Space Invaders-style game in C++23 + raylib, built for a university module at HTW Berlin and
continued past it. The approach throughout: local game first, then server, then multiplayer,
then a second client.

**Completed tasks live in [`ROADMAP-ARCHIVE.md`](ROADMAP-ARCHIVE.md)**, in full — every note and
post-mortem, nothing deleted. This file is the plan: what is left, and what to do next. Closed
entries appear here as one-line stubs so the numbering stays readable.

---

## Where things stand — 2026-10-24 (run on branch `l35`)

**Left for Levy this run:** the `GET /version` route in `server/src/main.cpp` (contract below).
Everything else in the run is wired and verified locally; the run lands as one PR once
`/version` exists, because the new CI step fails without it.

**`/version` contract.** `GET /version` → `200`, body exactly the value of the environment
variable `SUPERGAME_VERSION` (no trailing newline), or `unknown` if it is unset. The Dockerfile
sets that variable to the commit SHA (`GIT_SHA` build arg, passed by CI), and `docker-server`'s
last step polls `https://supergame.levynkeneng.dev/version` until it equals `$GITHUB_SHA` (30 ×
10 s). Answered by the old container, `/health` alone would pass during a failed redeploy; this
can't. Why it exists: on 2026-10-24 prod was unreachable after a redeploy (no domain set on the
Compose project — Traefik's own 404) while CI stayed green.

**Done this run (uncommitted):**
- **macOS `tests-asan` hang fixed** — the Darwin shell now uses `llvmPackages_22`. A one-line
  `-fsanitize=address` program hangs before `main` with nixpkgs' LLVM 20 and 21 (the old
  default) and runs with 22 and 23; `tests-asan` passes inside `nix develop` again (0.8 s). Also
  `stdenv.isLinux` → `stdenv.hostPlatform.isLinux` (deprecation).
- **L35** — see its entry.

**Next after this run:** **L36** (Embind shim, replaces the L35 stub), then L37/L38. L35–L38 are
local mode only. **L74 is deferred to sit with L39** (online mode on web); see its entry.

**Two manual checks left over from the last run** (nothing automated can cover them):
- Launch `supergame-client.exe` from `windows-client-release` on a real Windows machine (no
  Wine here). It builds clean and imports only Windows system DLLs, but has never been run.
- Play a local PvP game to its end screen and confirm "Team 1"/"Team 2" in their own colours —
  L68's (c) fix is compile-verified only, since the client has no test suite.

**Not blocking anything:** L70, L78 (native client internals — the web client puts the sim in
WASM and renders from TypeScript, so it inherits none of that structure), and L77.

**Just landed (`e765fe1`).** L68 closed. Also **C++23**: `CMAKE_CXX_STANDARD` 20 → 23 (C++26 is
still only partly implemented across this project's compilers). Verified on every preset —
`tests`, `tests-asan`, `server`, `client`, and now `windows-client-release` via `nix develop
.#mingw`, whose devShell fixed the old `cannot find -lmcfgthread` failure — all clean under
`-Wall -Wextra -Werror`, suites green (68 server + 22 shared) plain and sanitized. raylib still
compiles at `c++11`; it sets its own standard. Before that: L80/L81 (2v2 input overflow and
unwinnable spawns) and the `server-asan` preset, see the archive.

---

## Phases 1-3 — the graded module (done)

The local game (entities, bullets, enemy grid, AABB collision, boss, state machine, score and
lives), the client/server split with µWebSockets and a shared authoritative `GameSim`, Docker +
CI, the VPS deployment, and the visual/audio polish — shipped, graded, passed. Split between
Levy and a second contributor, Lynce, who is no longer on the project; from Phase 4 on, every
task is Levy's.

Every task from these phases: [archive](ROADMAP-ARCHIVE.md).

---

## Phase 4 — Game modes: Coop (1-4 players) & PvP (1v1 / 2v2) (no deadline)

**Goal:** An explicit mode choice, both local and online, instead of one fixed coop-vs-boss
game:

- **Local:** Coop (1-2 players, already shipped as L20) *or* PvP 1v1 (same machine,
  arrows vs. WASD, players fight each other instead of the boss).
- **Online:** Coop (**variable** headcount, 2-4 players — however many connect, no forced
  even/odd pairing, difficulty scales to whatever the actual count is) *or* PvP, chosen as
  either 1v1 or 2v2. A 2v2 team can be one machine bringing 2 local players (reusing L20's
  co-op input handling for a single connection), or two separate solo connections grouped
  together.

  **Decided (L58/L59 branch): online coop requires at least two separate connections**, not
  just two players. A single machine playing coop online — whether solo or as a same-keyboard
  duo — is pointless when local mode already covers it, so `CoopGame::thereIsEnoughPlayers()`
  gates on `player_sockets_.size() > 1` rather than on a player count. This replaces the
  earlier "1-4 players, however many connect" wording; the per-connection slot claim
  (`?slots=1|2`) still exists, it just can't be the *only* connection in the game. Note the
  client already matches this: its online count step asks how many players are on *this
  machine*, never the total game size, so nothing in the UI promises a solo online match.

Coop and PvP are two genuinely different rule sets sharing the same sim/session plumbing —
coop keeps today's boss-fight model (adapted to any count 1-4), PvP replaces it with
player-vs-player damage and a last-player/last-team-standing win condition. Both need to stay
selectable, not one replacing the other.

The original single `shared::MAX_PLAYERS` has since split into the three counts the wire
structs and sims actually need — `MAX_PLAYERS_COOP` (4), `MAX_PLAYERS_PER_TEAM` (2),
`MAX_PLAYERS_PER_CLIENT` (2) — plus `MAX_TEAMS` and the client's own
`MAX_PLAYERS_ON_THIS_MACHINE`. **All three gaps this phase originally identified are now
closed**: the mode split landed as a `CoopGameSim`/`PvPGameSim` type split (L28), the L21
difficulty tables cover all four player counts (L30), and one WS connection can now own two
player slots (L29 + L32) — so an online coop duo can share a keyboard and a 2v2 team can come
from a single machine, no longer requiring four separate connections. With the last structural
blocker gone, what remains in this phase is the UX that was waiting on it: L31's team *choice*
(only auto-balance shipped), L33's per-connection readying, and L34's same-team visual
distinction — all three now re-tracked together as **L74** in Phase 4.5, since they're better
done after that phase's roster/ownership cleanup than before it.

All tasks below are Levy's — no gameplay/visual split anymore.

| # | Task | Notes |
|---|------|-------|
| L27 | ✅ Mode + size selection UI | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
| L28 | ✅ `GameSim` mode flag governing collision + win condition | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
| L29 | ✅ Slot-claim handshake | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
| L30 | ✅ Variable-count coop difficulty scaling (1-4) | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
| L31 | **Team assignment for PvP 2v2** | How two duos/solos pair into Team A vs. Team B — a lobby-side "join team" pick, or auto-balance on connect (first arrival(s) to fill Team A, rest to Team B). *Partially landed with L28*: `PvPGame::addPlayer` auto-balances on connect, dropping each arrival into the first team with a free slot (so the auto-balance option is the one that shipped). Still missing: any way for a player to *choose* a team, which is what a duo joining as a pair actually needs. |
| L32 | ✅ Multi-slot `PlayerInput` per tick | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
| L33 | **Lobby UX: mode/size-aware matchmaking + ready gating** | `Screen::LOBBY` (L26) currently assumes one undifferentiated 2-player queue. Needs separate pools per (mode, size) so a coop seeker never lands in a PvP lobby and vice versa, a team-select affordance for 2v2 (from L31), and per-connection ready toggling so a duo readies up once, not twice per slot. *Partially landed with L28*: `GameManager` now keeps a coop pool plus one open-game pool per PvP team size, so a 1v1 seeker can't land in a 2v2 game and a coop seeker never sees either; `Screen::LOBBY` renders coop and PvP lobby updates separately (PvP grouped by team). Still missing: the team-select affordance and per-connection (rather than per-slot) readying, both of which depend on L29/L31. **Open question surfaced by the L58/L59 branch — how does a 3rd or 4th coop player ever actually get in?** `tryStart()` fires from both `addPlayers` and `setPlayersReady`, and coop's gate is satisfied at two ready connections, so a 3- or 4-player game only forms if the extra machines happen to connect *before* the first two both ready up; afterwards `addPlayers`' `!is_running_` check routes the next arrival into a fresh game. "First two ready connections start the match" is a defensible rule, but it was inherited from the ready-gate's timing rather than chosen, and it leaves L30's 3- and 4-player difficulty tables essentially unreachable in normal play. Needs a deliberate decision: either an explicit desired-size the lobby waits for, or a host-starts-the-match affordance, or accept first-two-ready and note that the higher counts are opportunistic. |
| L34 | **Mode-aware player visuals** *(was Ly15, Lynce's territory)* | Two visual schemes depending on mode: coop keeps/extends the existing per-player distinction (Ly7) up to 4 players; PvP needs team-colored ships instead (Team A one color family, Team B another, same-machine duo still distinguishable within their team's palette) plus a HUD that splits into two score/lives clusters in 2v2 rather than one shared readout. *Mostly landed with L28*, though via a **relational** scheme rather than team palettes: `Player::Type` (`MYSELF`/`PARTNER`/`ENEMY`) tints the shared ship texture WHITE/SKYBLUE/RED from the viewer's own perspective, and ships now rotate to `PlayerState.orientation` so a top-edge team visibly faces down. A second `Player::draw()` overload takes `std::array<TeamState, 2>` and renders the HUD grouped under "Team N" headers when `team_size > 1`. Still open: two players on the *same* team are both SKYBLUE and so indistinguishable from each other (the "duo distinguishable within their team's palette" part), and the same flattening hits coop once 3-4 players are in one game. |
| L56 | ✅ Sim coordinate system flipped to y-up | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
| L57 | ✅ Wire the sims' `removePlayer()` into the server's disconnect path | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |

---

## Phase 4.5 — Hardening & cleanup before the web client (no deadline)

**Goal:** clear the correctness and design debt from the Phase 4 rewrite *before* a second
client starts depending on the same server and wire format. Every entry came out of a full
review of the Phase 4 working tree.

**Two lessons from running it**, both earned the hard way and both worth re-reading before
picking up the next entry:

1. *"Small and independent" rarely survives contact.* L58, L59, L60 and L61 had to ship as one
   branch because each one's fix moved the same three structures; splitting them would have
   meant landing a state with two copies of every `PlayerConnection`. Check whether a task's
   framing holds against the others touching the same data.
2. *A default in the tests becomes an unexamined assumption in the code.* Every PvP case was
   1v1, which made `team_size == 1` invisible scaffolding — and three separate bugs (L80, L81,
   and the lobby overflow) all lived in the gap between a flat roster slot and a rank within a
   team, which only diverge at 2v2. When a probe finds a bug, write the failing case *first*,
   asserting intended behaviour rather than observed.

The full run log — what landed when, the bugs found during the review itself, and the ordering
arguments between entries — is in the [archive](ROADMAP-ARCHIVE.md).

| # | Task | Notes |
|---|------|-------|
| L58.1 | ✅ Build/CI/deploy hardening | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L58 | ✅ `PlayerConnection` ownership + `Game` handle discipline | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L59 | ✅ Sparse roster slots instead of swap-compaction | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L60 | ✅ Destroy games that empty out before starting | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L61 | ✅ Reject invalid handshake params instead of upgrading a zero-player connection | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L62 | ✅ Collapse the two player-id namespaces into one | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L63 | ✅ Interpolation keys on array slot, not entity identity — bullets *and* players | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L64 | ✅ Clear the remaining warnings, then turn on `-Werror` | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L66 | ✅ `virtual Game::buildLobbyUpdate()` instead of `dynamic_cast` + a parallel variant | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L67 | ✅ Collapse the Coop/PvP duplication behind a roster abstraction | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L68 | ✅ One accessor for live slots instead of count+array at every call site | [archive](ROADMAP-ARCHIVE.md#l68--one-accessor-for-live-slots-instead-of-countarray-at-every-call-site) |
| L69 | ✅ Binary wire format + hoist the per-socket `dump()` | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L70 | **Screen flow as a table, not two parallel switches** | Transitions live in a ~250-line `switch` in `handleInput()` plus a second one in `draw()`, with per-screen setup inlined at the transition site. That's what produced the missing `else` that made 2v2 unselectable and a `NAME_ENTRY` reachable without `initNameTextBoxes()`. A per-screen struct (`{onEnter, handleInput, draw}`) makes a missing transition or missing setup a visible hole. **(2026-10-15)** Two more arguments for this surfaced while scoping L66: a use-after-free in the `LOBBY` case (ESC frees `config_`, the `KEY_SPACE` branch below it still derefs the pointer cast before it), and the `std::variant<LocalPlay, OnlinePlay>` that would make an `OnlineConfig` paired with a `LocalSession` unrepresentable and retire `GameType type_` as a third spelling of local-vs-online - both belong to the same restructuring as the per-screen table. |
| L71 | ✅ Split the server into `supergame-server-lib` + a thin `main.cpp` | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L72 | ✅ PvP lobby rendering | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L73 | ✅ End / score screen | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L75 | ✅ Shared screen-layout helpers | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L76 | ✅ `.close` leaves a dangling `WsConnection *` on an already-over game | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L74 | **Per-connection ready, team select, and same-team visual distinction** | **Deferred (2026-10-22) — do the server half right before or alongside L39, the UI in `web/frontend/` only.** Three reasons: the web client first touches the lobby at L39, so there is no deadline before that; `client/` is retired at web parity (Phase 5), so a raylib team-select/ready UI now would be thrown away; and team select can be **additive** (optional `?team=` handshake param or a new `ClientMessageType`, auto-balance stays the default), so landing it later breaks no client. **Left, as of the 2026-10-22 audit:** (1) *Ready display* — server is already per-connection (`Game::setPlayersReady` sets every player on the `ws`), but `PlayerInfo::is_ready` and the client's `Players ready: n/m` count per slot, so a two-player client counts twice. Decide: keep the per-player field synced per connection (display-only change) or make ready per-connection on the wire. (2) *Team select* — not started; `PvPGame::addPlayers` takes the first team with room. A lobby-time switch must free the slot and re-insert into the other team's `teamSlotRange` (never patch `team_id` in place), and should clear ready so a switch after both teams readied can't start an unbalanced match. Server suite can cover it like the L80/L81 2v2 cases. (3) *Same-team distinction* — likely already done: `entity_designer.cpp:290-310` gives local duos `MAIN_PLAYER_COLOR` + `PLAYER_COLORS[…]`, coop colours by slot. Only gap: online PvP opponents are all `PLAYER_ENEMY_COLOR` — confirm in-game whether that is fine and close this part if so. <br><br>*Original entry:* The remainders of L31/L33/L34, now unblocked since one connection can own two slots. `setPlayersReady` keys on `ws` and sets the flag for every player on that socket, which is already per-connection behaviour — but the lobby still shows and counts it per slot. Still missing: any way to *choose* a team (only auto-balance-on-connect shipped), and two players on the same team are both drawn in one colour, so a same-machine duo can't tell their ships apart (same flattening hits coop at 3-4 players). |
| L77 | **One-command setup from a bare clone** | **Left:** an idempotent `make setup` (toolchain preflight + the submodule init in its exact non-recursive form — never `--recursive`, which pulls 1GB+ of BoringSSL/lsquic/fuzz corpus this project never uses, plus two *nested* inits for `uSockets` and mbedTLS's `framework`) that `client`/`server`/`all` depend on, so a bare clone needs one command rather than six README lines. Keep the logic in `scripts/setup.sh` with the Makefile as a thin wrapper. Exit non-zero naming the missing tool and its install command per platform. The `tests` target already needs no submodules at all, so it gets there for free. Per-module builds, the preset rewrite and both CI pipelines landed 2026-10-13. Full history: [archive](ROADMAP-ARCHIVE.md#l77--one-command-setup--per-module-builds-from-a-bare-clone). |
| L78 | **Client-side `dynamic_cast` removal (L66's other half)** | Nine `dynamic_cast<OnlineConfig *>` / `dynamic_cast<OnlineSession *>` sites in `client/src/game.cpp`, five of them unchecked derefs, left on their own branch by choice after the server half landed. Same duplicated-fact shape as the server had: `GameType type_` says local-vs-online, and the dynamic types of `config_`/`session_` say it again, with nothing checking that they agree. **Near-term fix:** two asserted accessors (`onlineConfig()`/`onlineSession()`) rather than cached `OnlineConfig *` / `OnlineSession *` members - a cached pointer dangles the moment `config_ = nullptr`, which the LOBBY/ESC branch does. **Structural fix, which belongs with L70:** one `std::variant<LocalPlay, OnlinePlay>` holding each config+session as a pair, making today's representable "OnlineConfig with a LocalSession" impossible and retiring `type_` as a third spelling. Do the accessors here only if L70 is not imminent; otherwise skip straight to the variant and close both together. Already fixed on the L66 branch and not part of this entry: the `LOBBY` use-after-free where ESC freed `config_` and the `KEY_SPACE` branch below it still dereferenced the pointer cast before it. |
| L79 | ✅ Make the server's inbound `.message` path testable | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L80 | ✅ 2v2 writes player input past the end of `PvPGame::inputs_` | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
| L81 | ✅ 2v2 spawns both teams on top of each other, so it cannot be won | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |

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
client logic that isn't in `shared/` and so can't be reused as-is). This sidesteps the
native client's networking entirely — no `ixwebsocket`/Emscripten-socket porting pain, just a
browser-native `WebSocket` speaking JSON `messages.h` already emits.

**Decided: `client/` gets retired once `web/` reaches parity, not maintained alongside it
indefinitely.** The native raylib binary stays authoritative while `web/` is being built;
once the web frontend matches it for the existing 2D game (Coop/PvP, Phases 1-4), `client/`
is dropped rather than kept in sync through Phase 6's 3D work. Reasoning: `web/wasm/` already
keeps the *simulation* (`shared/`) single-source regardless of how many rendering frontends
exist, but carrying two rendering frontends into a 3D rewrite would mean doing that work
twice for a browser target that already reaches every device `client/` does, plus ones it
can't (mobile, no install/update friction). See Phase 6's platform note for what this means
for raylib vs. WebGPU.

All tasks below are Levy's.

| # | Task | Notes |
|---|------|-------|
| L35 | ✅ **Emscripten toolchain + CMake wiring** | **Done (2026-10-24), branch `l35`.** `nix develop .#web` (emscripten **6.0.9**, node 24, `EM_CACHE` pointed at `~/.cache` since the nix store is read-only); `cmake/emscripten-toolchain.cmake` is a thin wrapper that finds `emcc` and includes Emscripten's own `Emscripten.cmake` (beside `emcc` in emsdk, under `../share/emscripten` in nix) with the same "enter the shell" error as the MinGW file; `BUILD_WEB` option (fatal outside Emscripten; under Emscripten client/server must be off and `server/` is skipped); `web` configure/build/test preset; `web/wasm/` with `supergame-web-sim` linking `supergame-shared` from a **stub** `main()` that L36 replaces. **Whole project builds with `-fwasm-exceptions`** under Emscripten: the test harness catches `FatalCheck` and nlohmann throws on bad JSON, and Emscripten's default is "throwing aborts". **The shared suite runs under node** (ctest via the toolchain's CROSSCOMPILING_EMULATOR): 22 tests / 114 checks, identical to native — so the sim behaves the same on wasm32, where `size_t`/`long` are 32-bit. A non-zero `main()` already becomes node's exit code in 6.0.9 (checked; no `EXIT_RUNTIME` needed). **CI:** new `test-web` job in `emscripten/emsdk:6.0.9` (same version as nix — bump together), no submodules needed, not in any `needs:`; verified locally in that image. <br><br>*Original entry:* New `cmake/emscripten-toolchain.cmake` + CMake preset (mirrors the existing `windows-client-release`/`cmake/mingw-w64-toolchain.cmake` cross-compile pattern), new `web/wasm/CMakeLists.txt` producing `supergame-web-sim` — links `supergame-shared` unchanged, gated behind a `BUILD_WEB` option so normal desktop/server builds are unaffected. |
| L36 | **WASM binding shim (JSON in/out over the wire format)** | Thin C++ layer exposing `GameSim::start()`/`step()` to JS (via Embind), taking a `PlayerInput` JSON string in and returning a `GameState` JSON string out — reusing `messages.h`'s existing `to_json`/`from_json`, no second marshaling scheme invented. |
| L37 | **`web/frontend/` scaffold** | TS project (Vite or similar) with Canvas2D rendering and keyboard→`PlayerInput` JSON input handling, structured to mirror the input/render split already established in `client/`. |
| L38 | **Unified `draw(gameState)` on the TS side** | One render function for both local (WASM `step()` output) and online (server message) `GameState` JSON — same source-agnostic principle as the desktop client's `draw()`. |
| L39 | **Online mode via native browser `WebSocket`** | TS client speaking the existing wire protocol directly to `supergame-server` (same `?name=`/`?players=` handshake as desktop, once L28/L29 land), plus a TS port of `OnlineSession`'s client-side interpolation lerp. **Do L74 with this** (team select + ready display: server half first, UI here only — see L74). |
| L40 | **Web asset pipeline** | Lazy-load audio behind a user-gesture "Play" button (browsers block autoplay pre-interaction anyway, so this is required, not optional), and re-encode `Backgroundsound.wav` (48MB) to a compressed web-friendly format — bundling the raw wav would make first load brutal. |
| L41 | **CI: build + deploy the web target** | New GitHub Actions job building the `web-sim` WASM preset + frontend bundle, mirroring the existing cross-compile job pattern — artifact upload/deploy gated to master pushes only, consistent with the rest of the pipeline's storage-conscious triggers. <br><br>**Decided (2026-10-24): hosting + routing.** The frontend is static files served by a Caddy/nginx container on the Dokploy VPS (multi-stage image: build WASM + TS bundle, serve `dist/`; `.wasm` must go out as `application/wasm`), not by `supergame-server`. **One domain, routed by path**: `supergame.levynkeneng.dev/ws` → game server, `/api` → the Phase 7 service, `/` → the static container (Traefik prefers the longer rule, so `/` is a safe catch-all). Same origin means Phase 7 auth needs no CORS and can use a plain `HttpOnly`/`SameSite=Strict` cookie. No server change needed: its WS route is `"/*"` (`server/src/main.cpp:70`); the client URL (`client/include/constants.h:14`) becomes `.../ws`. **Two things to handle at the switch:** `/health` and `/version` are exact routes, so give them their own rule to the game server (`/version` is what the post-deploy check polls); and v2.0.0 desktop clients connect to `/`, so either ship a new desktop build or add a `/` + `Upgrade: websocket` header rule. **Not two-level subdomains** (`game.supergame.…`): the domain is Cloudflare-proxied, so browsers get Cloudflare's Universal SSL certificate (`levynkeneng.dev` + `*.levynkeneng.dev` only, verified 2026-10-24) — the Nginx Proxy Manager cert only covers the Cloudflare→origin hop. Two-level names would need DNS-only (grey cloud) or a paid Advanced Certificate. |

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

**Platform note:** per Phase 5's decision, `client/` is expected to already be retired by the
time this phase starts, so the 3D renderer (L48) targets the web frontend, not raylib's
native 3D API — most likely via WebGPU, since raylib's Emscripten backend only reaches
WebGL2 and isn't a route to it. That shifts L48's original raylib-`Camera3D`/mesh learning
goal to whatever the web 3D stack ends up being — the API changes, the "gain real 3D
rendering experience" goal doesn't.

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
| L48 | **Third-person 3D camera + rendering (web)** | Web-side, not `client/` (see the Platform note above): a trailing third-person camera and billboards/simple 3D models rendered via the web frontend's 3D stack — most likely WebGPU. Biggest learning curve of this phase either way; the specific API changed (was raylib's `Camera3D`/mesh/lighting surface) but the goal of gaining real 3D rendering experience didn't. |

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
