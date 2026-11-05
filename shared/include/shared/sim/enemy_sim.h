#pragma once

#include "shared/constants.h"
#include "shared/sim/bullet_sim.h"
#include "shared/types.h"
#include <array>
#include <cstddef>
#include <mathlib.hpp>

namespace shared {
struct EnemySimState {
  mathlib::Vec2f position;
  EnemyType type;
  bool alive = false;
  float shooting_cooldown = 0;

  static constexpr float WIDTH = 32.0f;
  static constexpr float HEIGHT = 20.0f;
  static constexpr float SHOOTING_INTERVAL_MIN = 1.5f;
  static constexpr float SHOOTING_INTERVAL_MAX = 3.5f;

  /// Activates the first free slot in @p bullets as a new enemy-owned
  /// bullet fired downward from this enemy's current position.
  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;

  [[nodiscard]]
  mathlib::Rectanglef getHitBox() const;
};

/// Owns the whole enemy grid: entrance descent, side-to-side/step-down
/// movement, per-enemy shooting, and win/loss condition checks
/// (allEnemiesDefeated / reachedPlayer).
struct EnemiesPoolSimState {
  static constexpr auto SPACING_X = 40.0f;
  static constexpr auto SPACING_Y = 40.0f;
  static constexpr auto ROW_PITCH = EnemySimState::HEIGHT + SPACING_Y;
  static constexpr auto MIN_INITIAL_OFFSET_Y = SCREEN_HEIGHT + 30.0f;
  static constexpr auto FINAL_OFFSET_Y = SCREEN_HEIGHT - 50.0f;
  static constexpr auto INITIAL_DESCENT_SPEED = 70.0f;
  static constexpr auto CYCLE_SPEED = 200.0f;
  static constexpr auto DESCENT_SPEED = 35.0f;

  static constexpr std::array<std::pair<PlayerCount, EnemyCount>,
                              MAX_PLAYERS_COOP>
      ROWS_PER_PLAYERS_COUNT = {{{MAX_PLAYERS_COOP, MAX_ENEMY_ROWS},
                                 {MAX_PLAYERS_COOP - 1, MAX_ENEMY_ROWS - 2},
                                 {MAX_PLAYERS_COOP - 2, MAX_ENEMY_ROWS - 3},
                                 {MAX_PLAYERS_COOP - 3, MAX_ENEMY_ROWS - 5}}};

  EnemyCount active_rows{};
  std::array<EnemySimState, MAX_ENEMY_ROWS * ENEMY_COLS> enemies{};
  EnemyCount alive_enemies_count{};
  mathlib::Vec2f offset{};
  EnemyDirection direction{1};

  /// Resets the grid for a new match: marks the first `active_rows`
  /// rows (per @p playersCount, see ROWS_PER_PLAYERS_COUNT) alive and the
  /// rest dead, and resets formation position/direction.
  void init(PlayerCount playersCount);

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
