# CLAUDE.md

Guidance for Claude Code when working in this repository.

## Project

"Super Game" — a Space Invaders-style game in C++23, originally built for a university course at
HTW Berlin (module deadline August 12, met — graded, passed) and continued past it as a learning
project. Sole contributor: Levy, owning everything (formerly split with a second contributor,
Lynce, no longer on the project). See `ROADMAP.md` for the task breakdown and phase status —
keep it up to date as tasks complete; `ROADMAP-ARCHIVE.md` holds closed entries in full.

Where it stands: one authoritative simulation (`shared/`) runs verbatim in a headless server, in
the native raylib client's local mode, and in the browser via WebAssembly. Coop (1-4 players)
and PvP (1v1/2v2) work both locally and online, and the web client is deployed with mobile
touch controls. **Next: Phase 6, the 3D migration** (an on-rails corridor shooter, with 2D
kept as a selectable mode). The leaderboard (Phase 7) comes after it.

## Build & run

```bash
make                    # everything, Debug (make BUILD=release for Release)
make client / server / tests
make run-client         # against ws://localhost:9001
make run-server
make test               # shared/ + server/ suites via CTest
make help               # all targets
```

Every `make` target wraps a CMake preset of the same name (`cmake --list-presets`), each with its
own `build/<preset>` tree. The web build needs the nix shell: `nix develop .#web`, then
`make web-dev` (wasm + Vite on :5173), `make web-dist`, `make test-web`. Use `web-release` when
measuring performance — the Debug wasm sim is ~8x slower. First-time setup (submodules) is in
`README.md`.

## Architecture

- **`shared/`** — static library `supergame-shared`, no raylib: the headless, engine-agnostic
  core, linked into the client, server and wasm module. Included as `#include "shared/..."`.
  `helpers.h`, `wire.h` and `rnd_generator.h` sit at the top level (namespace `shared`).
  - `sim/` (namespace `shared::sim`) — `modes.h` holds `CoopGameSim<W>` and `PvPGameSim<W>`
    (separate types, not a mode flag; `start<N>`/`step<N>` take the player count as a template
    parameter) and `GameSim`, the variant of the concrete ones (`FlatCoop`, `FlatPvP`).
    `world.h` holds the `World` concept and `FlatWorld`, the 2D world: field bounds (centred
    on the origin), entrances, movement rules and the boss's fan. `entities/` has one `SimState` struct + update logic per
    entity (`player`, `bullet`, `enemies`, `boss`); they take values from the world, never the
    world itself, and never cross the wire.
  - `sim/messages.h` — the trimmed wire structs (`PlayerInput`, game states, lobby updates,
    `ServerMessageType`/`ClientMessageType`); not templated on the world. `wire.h` encodes/
    decodes them as JSON or msgpack; the format is negotiated per connection by WebSocket
    subprotocol.
  - `sim/types.h` — phase enums and id/count aliases. Vectors and hitboxes are
    `vendor/mathlib`'s (`Vec3f`, `Cuboidf`). `sim/constants.h` — counts and timings shared by
    sim and render.
  - `tests/` — sim and wire suites.
- **`server/`** — headless `supergame-server` over µWebSockets, split into a library plus a thin
  `main.cpp` so `server/tests/` can drive it with a fake connection. `GameManager` matches
  connections into `CoopGame`/`PvPGame` (both derive from `Game`), each owning a sim.
- **`client/`** — native raylib `supergame-client` (IXWebSocket for online play). Kept building
  and ported through sim changes, though the web client is the main one. Render-only entities,
  `GameScreen` enum in `client/include/types.h`, local or online `Session`.
- **`web/`** — `web/wasm/` exposes the sim to JS through Embind (`WebSim`, JSON across the
  boundary, plus world helpers like `FlatWorld.formationSlot` for the renderer);
  `web/frontend/` is TypeScript + Vite (rendering, input incl. touch, audio, menus).
  Deployed as Docker images behind Caddy by `.github/workflows/ci.yml`.

### Design conventions worth preserving

- **State and logic migrate together, per entity.** Don't move an entity's data without its
  update logic — that duplicates state and breaks single-source-of-truth.
- **`draw()` is source-agnostic.** Draw code takes the wire-shaped state, never `SimState`, so it
  renders identically from the local sim or a network snapshot.
- **Session/screen state lives above the sim, not inside it.** The client's `GameScreen`
  (C++ enum or TS union) governs *whether* the sim is stepped; the sim's phase enums track only
  game progression and ride along on the wire. Pause has no sim-side representation — the
  client just stops stepping and keeps drawing the last state.
- **Field vs. screen.** The field (`FlatWorld::FIELD_BOUNDS`) is the sim's; the screen size
  is each client's own constant. Renderers map field → screen in one `toScreen` (measured
  from the field's `min.x`/`max.z`, times a scale) and scale sizes the same way.
- **Hitbox vs. texture size.** The authoritative hitbox constant lives in `shared/`; any visual
  scale factor stays client-side.
- **Entity sizes are full extents** (`Vec3f SIZE`, as mathlib's `fromCenterAndSize` expects).
  Halves are derived (`HALF_SIZE = SIZE / 2`), never set by hand.
- **Positive `orientation` is counter-clockwise as seen on the Flat screen** (x right, z up),
  which is a rotation around **-y** (`rotatedAroundAxis(-unitY, θ)`), not +y.
- **No mode checks inside update functions.** Rule differences (Coop/PvP now, Flat/Rail in
  Phase 6) are separate types or composed parts, not `if (mode)` branches.

## Working agreement — IMPORTANT

**Levy writes all gameplay/simulation/entity logic himself.** This is a learning exercise, not a
delegated implementation task. When asked a design or "how would you do X" question:

- Explain, point out gaps, and give illustrative snippets **in chat**.
- **Do not** `Write`/`Edit` source files — C++ headers and `.cpp` in `shared/`, `server/`,
  `client/`, `web/wasm/`, or the TypeScript logic in `web/frontend/src/` — unless explicitly
  asked to implement/apply a change.
- If it's ambiguous whether a request means "explain" or "implement," ask first.

**Exceptions — fine to edit directly, no need to ask:**
- `CMakeLists.txt`, `CMakePresets.json`, `vendor/*.cmake`, `cmake/*.cmake` — build wiring.
- `ROADMAP.md`, `ROADMAP-ARCHIVE.md` and this `CLAUDE.md` — project documentation/tracking.

Reading/building/running the code to diagnose bugs or verify claims is always fine — the
restriction is on writing to logic files, not on investigating them.
