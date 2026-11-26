# Project Roadmap — The Super Game

**Start:** June 18 | **Course deadline:** August 12 (met — graded, passed) | Sole contributor: Levy

A Space Invaders-style game in C++23 + raylib, built for a university module at HTW Berlin and
continued past it. The approach throughout: local game first, then server, then multiplayer,
then a second client.

**Completed tasks live in [`ROADMAP-ARCHIVE.md`](ROADMAP-ARCHIVE.md)**, in full — every note and
post-mortem, nothing deleted. This file is the plan: what is left, and what to do next. Closed
entries appear here as one-line stubs so the numbering stays readable.

---

## Where things stand — 2026-11-26 (run on branch `l85`)

**Last run: L85 — origin at the field's centre, field split from screen. Done; PR open.**
All four suites pass, and matches were played on both clients. One gameplay change: the
formation now starts centred, 32 units right of before. Full record in
the [archive](ROADMAP-ARCHIVE.md#phase-6--3d-migration-on-rails-corridor-shooter-no-deadline).

**Next run: not chosen.** Candidates: L44 (Rail steering, the next Phase 6 step), or the PvP
gameplay design together with L84 (a wider field — now just a `FIELD_BOUNDS` and screen-size
change). L33's open question remains a small decision for any run.

**Decided (2026-11-10), still standing:** the game and its C++ come first, Phase 7 (the
leaderboard) waits until after Phase 6, and `client/` stays and is ported through sim changes.

**Local setup note:** the `.#web` nix shell isn't a GC root (direnv only pins the default
shell), so `nix-collect-garbage` deletes emscripten. Pin it once with
`nix develop .#web --profile .direnv/web-profile --command true`. Test phone performance with
the Release wasm (`--preset web-release -B build/web`): the Debug sim is ~8x slower.

**Manual checks still open** (nothing automated covers them):
- Launch `supergame-client.exe` from `windows-client-release` on a real Windows machine. It
  builds clean and imports only Windows system DLLs, but has never been run.
- Play a local PvP game to its end screen and confirm "Team 1"/"Team 2" in their own colours
  (L68's (c) fix is compile-verified only — the client has no test suite).

**Not blocking anything:** the boss's successive shots aim from `position` rather than the bullet's spawn point (`boss.cpp`, barely visible); an assert for `PvPGame`'s `team_count ≤ MAX_TEAMS` (handshake-
validated today); L77; L70/L78 (native client internals — `client/` is retired at web parity).

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
| L31 | ✅ Team assignment for PvP 2v2 | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
| L32 | ✅ Multi-slot `PlayerInput` per tick | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
| L33 | **Lobby UX: how a 3rd/4th coop player gets in** | **Left: one decision.** Pools per mode/size landed with L28, team select and per-player ready with L74. Coop starts as soon as two connections are ready, so a 3rd or 4th player only gets in by connecting before that, and L30's 3- and 4-player tables are rarely reached. Options: the lobby waits for a chosen size, a host starts the match, or accept "first two ready" and call the higher counts opportunistic. <br><br>*Entry as it stood:* `Screen::LOBBY` (L26) currently assumes one undifferentiated 2-player queue. Needs separate pools per (mode, size) so a coop seeker never lands in a PvP lobby and vice versa, a team-select affordance for 2v2 (from L31), and per-connection ready toggling so a duo readies up once, not twice per slot. *Partially landed with L28*: `GameManager` now keeps a coop pool plus one open-game pool per PvP team size, so a 1v1 seeker can't land in a 2v2 game and a coop seeker never sees either; `Screen::LOBBY` renders coop and PvP lobby updates separately (PvP grouped by team). Still missing: the team-select affordance and per-connection (rather than per-slot) readying, both of which depend on L29/L31. **Open question surfaced by the L58/L59 branch — how does a 3rd or 4th coop player ever actually get in?** `tryStart()` fires from both `addPlayers` and `setPlayersReady`, and coop's gate is satisfied at two ready connections, so a 3- or 4-player game only forms if the extra machines happen to connect *before* the first two both ready up; afterwards `addPlayers`' `!is_running_` check routes the next arrival into a fresh game. "First two ready connections start the match" is a defensible rule, but it was inherited from the ready-gate's timing rather than chosen, and it leaves L30's 3- and 4-player difficulty tables essentially unreachable in normal play. Needs a deliberate decision: either an explicit desired-size the lobby waits for, or a host-starts-the-match affordance, or accept first-two-ready and note that the higher counts are opportunistic. |
| L34 | ✅ Mode-aware player visuals | [archive](ROADMAP-ARCHIVE.md#phase-4--game-modes-coop-1-4-players--pvp-1v1--2v2-no-deadline) |
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
| L74 | ✅ Per-player ready, team select, same-team distinction | [archive](ROADMAP-ARCHIVE.md#phase-45--hardening--cleanup-before-the-web-client-no-deadline) |
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

**Reversed (2026-11-10): `client/` stays** after all (see the status section). It is kept
building and gets ported through Phase 6's sim changes; the 3D renderer is still web-first.
*Original decision:* **`client/` gets retired once `web/` reaches parity, not maintained alongside it
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
| L35 | ✅ Emscripten toolchain + CMake wiring | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L36 | ✅ WASM binding shim (JSON in/out over the wire format) | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L37 | ✅ `web/frontend/` scaffold | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L38 | ✅ Unified `draw(gameState)` on the TS side | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L39 | ✅ Online mode via native browser `WebSocket` | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L40 | ✅ Web audio | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L41 | ✅ CI: build + deploy the web target (Caddy in front of the server) | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L82 | ✅ Per-connection wire format, negotiated at the handshake | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L83 | ✅ Mobile: responsive layout + touch controls | [archive](ROADMAP-ARCHIVE.md#phase-5--web-client-wasm-no-deadline) |
| L84 | **A wider field for phones** | **Undecided; not scheduled.** On a phone the 5:4 field (1000×800, `shared/`) is height-bound: an iPhone 14 in landscape fits at most 488×390, about 58% of the width, with the controls in the side bars. L83 got it to that cap; more size needs a different field shape. Options: a wider field for everyone (changes enemy formation spacing, movement range, the desktop client's window, and online fairness stays intact since all clients share it), or per-device shapes (unfair online: players see different amounts of the field). Decide together with the PvP gameplay design, since both change the match. |

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

**Sequencing note:** Phase 4's collision work has already shipped in 2D, so the migration
generalizes it (L46) instead of preceding it. Phase 5's design is unaffected: the WASM shim
and the binary wire format carry a third coordinate as one more field.

**Platform note:** `client/` stays (2026-11-10) and is ported through the sim changes, but the
3D renderer (L48) targets the web frontend, not raylib's
native 3D API — most likely via WebGPU, since raylib's Emscripten backend only reaches
WebGL2 and isn't a route to it. That shifts L48's original raylib-`Camera3D`/mesh learning
goal to whatever the web 3D stack ends up being — the API changes, the "gain real 3D
rendering experience" goal doesn't.

**Decided (2026-11-01): 2D stays, as a selectable mode beside 3D.** Mode select gets a
dimensionality axis next to Coop/PvP. 3D-specific features are still undesigned. The shape:

- **One sim, `Vec3D` everywhere, not a separate 3D sim.** Bullet pool, collision, roster,
  inputs, scoring and the wire format are written once.
- **Axes have fixed meanings in both modes:** `x` = lateral, `y` = height, `z` = depth (the
  advance axis: formations approach and bullets fly along it). **2D is the 3D game seen from
  above with `y` pinned to 0.** Not "today's 2D plus a `z`": the sim's current `y` (the advance
  axis since L56) becomes `z`, so shared systems never need to know which mode they're in. The
  2D renderer draws `(x, z)`. Enforce `y == 0` in one place in flat mode, not per call site.
- **Rules differ per dimension, so split them like Coop/PvP (L28):** player bounds (1-DOF vs.
  2-DOF), formation advance (grid steps vs. the rail), boss fan (arc vs. cone), 3D-only
  features. **Compose, don't inherit:** Coop/PvP × Flat/Rail is four combinations, so the
  mode rules and the world rules should be independent parts (template parameters or strategy
  members), not four classes. No `if (is3D)` inside update functions (the L70 problem).
- **Open:** which way do PvP teams face on a rail? (Doesn't block L43: it becomes a
  `W::teamSpawn(team)` on the world.)

**Decided (2026-11-10): the world is a template parameter, and entities never see it.**
- `template <World W> class CoopGameSim` / `PvPGameSim`, with `World` a concept. `FlatWorld` and
  `Rail` are structs of static members. `GameSim` becomes a 4-way variant. Keep the sims' code in
  `game_sim.cpp` through explicit instantiation (`template class CoopGameSim<FlatWorld>;` etc.).
  A runtime strategy was rejected: the world never changes mid-match, and the variant already
  picks the type once.
- **Entities take values, not the world.** E.g. `player.step(W::moveDirection(buttons),
  W::PLAYER_BOUNDS, ...)`: move along a direction, then clamp to a box. Flat's
  `PLAYER_BOUNDS` has zero height, which is the one place `y == 0` is enforced. The boss gets
  its volley's directions from the world instead of computing angles.
- **What the world supplies** (as refined 2026-11-18 to 11-22):
  - `FIELD`, a `Cuboidf` for the whole playing volume (Flat: zero height; Rail: from just
    behind the player to the spawn distance). `PLAYER_BOUNDS` (the field shrunk by the ship's
    half-size, `z` collapsed to the player's depth) and `BULLET_BOUNDS` derive from it.
    Other borders (the boss's sweep, the formation's walls) are derived from `FIELD` inside the
    world's own functions and stay out of the concept.
  - `moveDirection(buttons)`, built with a free `axis(buttons, negative, positive)` (-1/0/+1).
    Rail must normalise diagonals, or they're √2 faster.
  - `spreadDirections(from, std::span<Vec3f> out)`: fills a buffer the boss owns (a
    `std::array` of `MAX_SPREAD_SHOT_BULLETS`, viewed with `.first(count)`) from the spawn
    point at the boss's front. The boss then pairs each direction with the next free bullet.
    Directions, not bullets, so the world does pure geometry.
  - `moveFormation(motion, aliveExtents, dt) -> Vec3f`: returns a displacement, which the sim
    adds to the pool's `offset`. Flat: today's sweep and step toward the player at a wall.
    Rail: a continuous approach. `offset` (the far-left enemy's centre, the formation's anchor)
    stays in the pool; `aliveExtents()` is computed per tick from alive hitboxes, never stored.
    Keep today's order: extents from last tick's positions, then move, then reposition.
  - `BossMotion` + `moveBoss(motion, position&, phase, dt)` (below).
  - **Spawn and entrance positions as values:** `PLAYER_SPAWN`; an `Entrance {start, end,
    speed}` for the boss and `formationEntrance(active count along z)` for the formation (start
    bottom-anchored: `(count - 1) * pitch.z` behind). The entities' `stepEntrance(target,
    speed, dt)` moves toward the target and snaps on arrival, so it doesn't care about the
    axis. Curved entrances, if wanted later: a Bézier path with a duration instead of a speed.
    Speeds unrelated to the world's distances (bullets, cooldowns, boss phases) stay on the
    entities.
  - `FORMATION`, the formation's shape (below).
- **Already world-agnostic, given the fixed axes:** 3D box collision (all `y = 0` in Flat,
  so the `y` overlap always holds), bullet movement, `reachedPlayer` (formation `z` ≤ player
  `z`), the shot direction (`+z` turned by `orientation`).
- **The world is a per-match choice, online too:** lobby pools keyed by (mode, world, size),
  plus a handshake param.
- **How entities get world behaviour (2026-11-18):** the sim computes values and passes them
  (`moveDirection`, the bounds, formation slots precomputed once in `start()`). Behaviour the
  entity triggers itself goes in as a plain function pointer (`&W::spreadDirection`), keeping
  entities non-templates with their code in `.cpp`. A member template on the entity is the last
  resort.
- **Hitboxes become `mathlib::Cuboidf` (2026-11-18), bullets included.** They are the only
  shape that keeps Flat identical to today, and give one box-vs-box test for everything. `y`
  sizes only matter on Rail; bullets get a square cross-section (4 × 4 × 12). No tunnelling at
  current speeds (≤ 15 units/tick vs. ≥ 30 units of overlap window). If Rail changes that,
  switch bullets to a capsule swept from last tick's position.

**Decided (2026-11-18): the Rail boss flies a figure-eight in the `x`–`y` plane** (a Lissajous
curve: `x = A·sin(a·t)`, `y = B·sin(b·t + φ)`, `a:b = 1:2`), at a fixed `z`. It gets faster
with each fight phase: an ellipse in phase 1, then a figure-eight, faster in phase 3.
- **Constraint:** player shots fly straight along `+z`, so the path must stay within the
  player's steerable `x`/`y` area (minus the boss's hitbox margin), or the boss is unhittable.
- **The movement state belongs to the world, not to the boss.** Flat needs a sweep direction,
  Rail an elapsed time. Each world defines a `BossMotion` struct and a
  `moveBoss(motion, position, phase, dt)`; the sim holds a `typename W::BossMotion` next to
  `boss_` and moves the boss itself, before `boss_.step()` fires. `BossSimState` loses
  `direction` and keeps health, phase and patterns. Flat's `moveBoss` is today's sweep, moved
  out unchanged. The enemy formation's `offset`/`direction` can follow the same pattern
  (`advanceFormation`); its wall bounce then needs the pool to expose its alive extents.
- **Rail's boss entrance** arrives from far `z` instead of descending in `y`: just a different
  `Entrance` value.
- **Later ideas, not planned:** a telegraphed charge toward the player in phase 3; drifting
  toward the player as a short phase.

**Decided (2026-11-22): each world defines its formation's shape, counted per world axis.**
- `FormationShape { count, pitch }`: enemies along `x`, `y`, `z`, plus centre-to-centre
  spacing per axis. Flat: `{10, 1, 8}` (today's 8 rows are its `z` count). Rail: e.g.
  `{10, 4, 2}`, a wall of 4 rows in height, 2 layers deep. Both total 80.
- **The slot formula is the same in both worlds** and lives on the shape:
  `{ix * pitch.x, -iy * pitch.y, -iz * pitch.z}` (later slices go down and toward the player).
  So `formationSlot` is no longer a world function, and "nobody ahead of me" (today's
  `idx + ENEMY_COLS` shooting rule) is always the next index along `z`: the next row in Flat,
  the next layer in Rail. The pool stays a non-template: it gets the shape as a value in
  `init`.
- **Capacity:** one `MAX_ENEMIES = 80` replaces `MAX_ENEMY_ROWS * ENEMY_COLS` for the pool and
  the wire's enemy array, with `static_assert(W::FORMATION.size() <= MAX_ENEMIES)` per world.
  Raise it deliberately if a world needs more.
- **Open (decide at L45): difficulty per world.** `ROWS_PER_PLAYERS_COUNT` trims Flat's `z`
  slices; on Rail, 2 layers is too coarse, so it would trim `y` rows instead. Either a table plus
  the axis it trims, or `W::activeEnemies(playerCount, ix, iy, iz) -> bool`. For L43, move
  today's table into `Flat` unchanged.

**Idea (2026-10-25), not yet a task: player counts from formulas, not tables.** Today the caps
are fixed (`MAX_PLAYERS_COOP`, `MAX_TEAMS × MAX_PLAYERS_PER_TEAM`) and enemy count/difficulty
come from per-count values. Idea: scale difficulty with formulas in n, and let each consumer
choose its maximum, which the `N` of `start<N>`/`step<N>` already half-does. 3D is the natural
time for it: per-player cameras or split screen remove the 2D screen-width limit (PvP lines a
team up side by side, `game_sim.cpp:34`). **Keep the fixed caps and raise them deliberately**
rather than going to dynamic containers. The heap isn't the concern (one allocation in
`start()`, none per tick); the limits that remain are the fixed `BulletsPool`, wire state
growing per player per tick, balance at untested n, and split screen's n renders per frame.
**Constraint to keep:** local seats (`MAX_PLAYERS_ON_THIS_MACHINE`, e.g. 5 with gamepads) can
exceed a mode's cap; that's already a compile error at `start`/`step` (`requires(N <= …)`), so
the menu offers only the counts the mode allows and copies that many ids into the sim's array.

All tasks below are Levy's.

| # | Task | Notes |
|---|------|-------|
| L42 | **`Vec3D` primitives in `shared/`** | **Mostly done by the `vendor/mathlib` switch:** `mathlib::Vec3f` exists (incl. `rotatedAroundAxis`). **Left:** only a cone-spread helper for L47, if mathlib doesn't cover it. The 3D box landed (`Cuboidf`/`OrientedCuboidf` with `intersects`, used by L43). *Original entry:* Add a 3D vector type alongside the existing `Vec2D` (`math_utils.h`), generalizing the angle/rotation math already built for the boss's spread-shot fan (`Vec2D::rotated`, `angle_between`, `dot_product` — L21) to 3D (e.g. spreading a bullet volley across a cone instead of a 2D arc). |
| L43 | ✅ Migrate sim positions `Vec2D` → `Vec3D` (with `World`/`Flat`) | [archive](ROADMAP-ARCHIVE.md#phase-6--3d-migration-on-rails-corridor-shooter-no-deadline) |
| L85 | ✅ Sim origin at the field's centre, field split from screen | [archive](ROADMAP-ARCHIVE.md#phase-6--3d-migration-on-rails-corridor-shooter-no-deadline) |
| L44 | **Player steering: 2-DOF movement plane** | Today `PlayerSimState::POSITION_Y` is fixed — the player only ever moves in X. On-rails 3D needs real X/Y steering within a bounded plane a fixed distance in front of the camera (Z stays constant for the player; only the *world* advances in Z). Needs a new `UP`/`DOWN` bit on `PlayerInput.buttons` alongside the existing left/right/fire bits. |
| L45 | **World/enemy Z-advance ("the rail")** | **Shape decided (2026-11-22, Phase 6 above):** a wall facing the player (columns along `x`, rows along `y`), in **layers along `z`**, e.g. 10 × 4 × 2. **Left from L43:** the 2026-11-22 `FormationShape`/`MAX_ENEMIES` change (today `formationSlot` is a `FlatWorld` function and the pool is `ENEMY_COLS * MAX_ENEMY_ROWS`). **Left to design:** Rail's difficulty table; stagger the back layers by half a column/row so they show through the gaps; rows of the back layers start shooting once the one ahead dies (falls out of the shared `z` rule); `reachedPlayer` uses the nearest alive layer (`aliveExtents().min.z`). *Original entry:* Reinterpret the existing enemy row/column formation (`enemy_sim.h`) as an (X,Y) formation plane spawned at a far Z that shrinks toward the player over time, replacing the current "step down the 2D grid" logic — keeps the existing grid/wave structure, just adds depth instead of a full rewrite. |
| L46 | **3D collision (AABB → 3D bounding box)** | **Mostly done by L43:** every hitbox is a `mathlib::Cuboidf` and the sim's one test is `mathlib::intersects`. **Hitbox size decided (2026-11-26):** the player keeps `HITBOX_SCALE = 0.92` on the full 40 (36.8 wide), up from the old 18.4 (Levy). **Left:** check the `y` extents once Rail gives entities height (bullets 4 × 4 × 12), and the capsule fallback if Rail speeds ever tunnel. *Original entry:* Generalize the hitbox constants (currently a single `SIZE` per entity, e.g. `PlayerSimState::SIZE`) to width/height/depth, and extend the AABB collision check used throughout `GameSim` to 3 axes. |
| L47 | **Boss spread-shot fan in 3D** | The existing angle-fan bullet pattern (L21) generalizes from a 2D arc to a 3D cone using the new `Vec3D` rotation math from L42. |
| L48 | **Third-person 3D camera + rendering (web)** | Web-side, not `client/` (see the Platform note above): a trailing third-person camera and billboards/simple 3D models rendered via the web frontend's 3D stack — most likely WebGPU. Biggest learning curve of this phase either way; the specific API changed (was raylib's `Camera3D`/mesh/lighting surface) but the goal of gaining real 3D rendering experience didn't. |

---

## Phase 7 — Online Leaderboard with Accounts (no deadline)

**Deferred (2026-11-10): after Phase 6.** A nice-to-have; the game itself comes first.

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
