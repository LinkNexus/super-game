# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Project

"Super Game" — a Space Invaders-style game in C++20 + raylib, built for a university course at
HTW Berlin. Deadline: August 12. Two contributors: Levy (gameplay/simulation/networking) and
Lynce (visuals/audio/UI polish). See `ROADMAP.md` for the full task breakdown and phase status —
keep it up to date as tasks complete.

The project is moving from a local single-machine game (Phase 1, done) to a networked
client/server model (Phase 2, in progress): one authoritative simulation shared verbatim between
a headless server and the client's own "local mode," with only trimmed state crossing the wire
for online play.

## Build & run

```bash
cmake --preset debug
cmake --build build/debug
./build/debug/bin/supergame-client      # Linux
build\debug\bin\supergame-client.exe    # Windows
```

Release preset is `release` instead of `debug`. First-time setup (submodules) is documented in
`README.md`.

## Architecture

Three CMake targets, each its own subdirectory with `include/` + `src/`:

- **`shared/`** — static library `supergame-shared`, linked into both `client` and `server`. No
  raylib dependency — this is the headless, engine-agnostic core. Layout is the conventional
  nested form: `shared/include/shared/*.h` + `shared/src/*.cpp`, included elsewhere as
  `#include "shared/..."` (chosen deliberately over a flat `shared/*.h` layout for readability
  as the project grows).
  - `shared/include/shared/sim/` — the actual simulation: `GameSim` (headless authoritative game
    loop, `step(inputs, dt)`) plus one `SimState` struct + update logic per entity
    (`player_sim`, `enemy_sim`, `bullet_sim`, `boss_sim`). These structs hold full internal
    bookkeeping (cooldowns, phase, pattern, velocity) and are never sent over the network.
  - `shared/include/shared/messages.h` — the trimmed wire format (`PlayerInput`, `GameState`,
    etc.). Only these cross the network in online mode; `SimState` stays server/local-process
    only. `GameSim::step()` takes internal state in and writes a `GameState` out each tick.
  - `shared/include/shared/constants.h` — dimensions/speeds shared between sim and render (e.g.
    entity hitbox sizes needed for both collision and drawing).
- **`client/`** — raylib binary `supergame-client`. Entities here (`Player`, `Boss`, `Enemy`) are
  render-only: `Texture2D` + `draw(const shared::XState&)`, no simulation logic, no raylib input
  calls outside `Game::getPlayerInputs()`/`Game::handleInput()`. `Game` owns a `shared::GameSim`
  and runs it in-process for local play.
- **`server/`** — headless binary `supergame-server`, no raylib. Will run `GameSim` authoritatively
  over WebSockets (µWebSockets, vendored under `vendor/uWebSockets`).

### Design conventions worth preserving

- **State and logic migrate together, per entity.** Don't split moving an entity's data to
  `shared/` from moving its update logic — that duplicates state and breaks single-source-of-truth.
- **`draw()` is source-agnostic.** Client draw functions take the minimal wire-shaped state
  (`shared::PlayerState`, not the internal `SimState`), so they render identically regardless of
  whether the state came from local sim or a network snapshot.
- **Session/screen state lives above the sim, not inside it.** `Screen` (`MENU`, `PLAYING`,
  `PAUSED`, `GAME_OVER`, `WIN`) is a client-only enum in `client/include/game.h` — it governs
  *whether* `Game` calls into the sim at all. `GameSim`'s own `GamePhase` (`ENEMIES_ENTRANCE`,
  `FIGHT_ENEMIES`, `BOSS_ENTRANCE`, `FIGHT_BOSS`, `WON`, `GAME_OVER`) only tracks sim-relevant
  progression and rides along in `GameState.phase` for the wire. Pause has no sim-side
  representation at all — `Game::run()` simply skips calling `sim_.step()` while paused and keeps
  drawing the last known state.
- **Hitbox vs. texture size.** For entities with real sprites where visual size may differ from
  collision size (e.g. `Boss`), the authoritative hitbox constant lives in `shared/`; any
  client-only visual scale factor stays in `client/`.

## Working agreement — IMPORTANT

**Levy writes all gameplay/simulation/entity logic himself.** This is a learning exercise for a
university course, not a delegated implementation task. When asked a design or "how would you do
X" question about game/sim code:

- Explain, point out gaps, and give illustrative snippets **in chat**.
- **Do not** `Write`/`Edit` actual source files — headers, `.cpp`, `shared/` sim code,
  `client/` entity or `game.cpp` files — unless explicitly asked to implement/apply a change.
- If it's ambiguous whether a request means "explain" or "implement," ask first.

**Exceptions — fine to edit directly, no need to ask:**
- `CMakeLists.txt`, `CMakePresets.json`, `vendor/*.cmake` — build wiring, not game logic.
- `ROADMAP.md` and this `CLAUDE.md` — project documentation/tracking, not game logic.

Reading/building the code to diagnose bugs or verify claims is always fine — the restriction is
specifically on writing to logic files, not on investigating them.
