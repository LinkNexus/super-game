#pragma once

#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"

namespace shared {
/// Top-level progression state for a match, independent of client-only
/// screen/menu state (see `Screen` in the client, which governs *whether*
/// GameSim is stepped at all - GamePhase only tracks sim-relevant
/// progression and rides along in GameState::phase for the wire).
enum class GamePhase : uint8_t {
  ENEMIES_ENTRANCE,
  FIGHT_ENEMIES,
  BOSS_ENTRANCE,
  FIGHT_BOSS,
  WON,
  GAME_OVER
};

/// Headless, engine-agnostic authoritative game loop. Runs identically in
/// the server process (one instance per match) and in the client's local
/// mode (in-process, no networking) - this is the single source of truth
/// for all gameplay logic, with no raylib or networking dependency.
class GameSim {
public:
  /// Starts a new match for the given players. Each non-empty slot in
  /// @p player_ids becomes one active player; the resulting count scales
  /// enemy row count and boss health/attack patterns for the rest of the
  /// match.
  void start(std::array<std::optional<uint32_t>, MAX_PLAYERS> &player_ids);

  /// Advances the simulation by one fixed tick: applies @p inputs (matched
  /// to players by id, not array slot), steps whichever phase is currently
  /// active, resolves collisions, and writes the resulting snapshot into
  /// @p state for the wire/local rendering.
  void step(GameState &state,
            const std::array<std::optional<PlayerInput>, MAX_PLAYERS> &inputs,
            float dt);

private:
  void checkCollisions();
  void setGameState(GameState &state);

  std::array<std::optional<PlayerSimState>, MAX_PLAYERS> players_{};
  std::array<BulletSimState, MAX_BULLETS> bullets_pool_{};
  EnemiesPoolSimState enemies_pool_{};
  BossSimState boss_{};
  GamePhase phase_{};
  uint8_t players_count_{};
};
} // namespace shared
