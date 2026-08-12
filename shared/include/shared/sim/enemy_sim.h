#pragma once

#include "shared/constants.h"
#include "shared/sim/bullet_sim.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace shared {
enum class EnemyType : uint8_t {
  TYPE_1,
  TYPE_2,
};

/// One slot in the fixed `MAX_ROWS * COLS` enemy grid. Inactive
/// (`alive == false`) slots are simply skipped by simulation and rendering,
/// rather than the grid being resized - this is how enemy count scales
/// down for solo play without touching the wire format.
struct EnemySimState {
  Vec2D position;
  EnemyType type;
  bool alive = false;
  float shooting_cooldown = 0;

  static constexpr float WIDTH = 32.0f;
  static constexpr float HEIGHT = 20.0f;
  static constexpr float CYCLE_SPEED = 200.0f;
  static constexpr float DESCENT_SPEED = 35.0f;
  static constexpr float SHOOTING_INTERVAL_MIN = 1.5f;
  static constexpr float SHOOTING_INTERVAL_MAX = 3.5f;

  /// Activates the first free slot in @p bullets as a new enemy-owned
  /// bullet fired downward from this enemy's current position.
  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;
};

/// Owns the whole enemy grid: entrance descent, side-to-side/step-down
/// movement, per-enemy shooting, and win/loss condition checks
/// (allEnemiesDefeated / reachedPlayer).
struct EnemiesPoolSimState {
  static constexpr float INITIAL_OFFSET_Y = -270.0f;
  static constexpr float FINAL_OFFSET_Y = 50.0f;
  static constexpr float INITIAL_DESCENT_SPEED = 70.0f;
  static constexpr int MAX_ROWS = 5;
  static constexpr int COLS = 10;
  static constexpr float SPACING_X = 40.0f;
  static constexpr float SPACING_Y = 40.0f;
  /// Maps active player count to how many of `MAX_ROWS` rows start alive,
  /// so difficulty scales with player count. Falls back to `MAX_ROWS` for
  /// any player count not listed here.
  static constexpr std::array<std::pair<uint8_t, int>, MAX_PLAYERS>
      ROWS_PER_PLAYERS_COUNT = {
          {{MAX_PLAYERS, MAX_ROWS}, {MAX_PLAYERS - 1, MAX_ROWS - 2}}};

  int active_rows;
  std::array<EnemySimState, MAX_ROWS * COLS> enemies;
  std::size_t alive_enemies_count;
  float offset_x;
  float offset_y;
  int direction;

  /// Resets the grid for a new match: marks the first `active_rows`
  /// rows (per @p playersCount, see ROWS_PER_PLAYERS_COUNT) alive and the
  /// rest dead, and resets formation position/direction.
  void init(uint8_t playersCount);

  /// Advances the entrance descent from off-screen to `FINAL_OFFSET_Y`.
  void stepEntrance(float dt);

  /// @return true once the entrance descent has reached `FINAL_OFFSET_Y`.
  bool isEntranceComplete();

  /// Advances one tick of the fight: side-to-side movement with a
  /// step-down on hitting a screen edge, and enemy shooting.
  void step(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool);

  /// @return true once every enemy in the grid has been marked dead.
  bool allEnemiesDefeated();

  /// @return true once any enemy has descended low enough to reach the
  /// player's row - an immediate game-over condition.
  bool reachedPlayer();

private:
  void stepEnemy(EnemySimState &enemy, std::size_t idx);
};

} // namespace shared
