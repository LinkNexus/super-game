#pragma once

#include "shared/sim/constants.h"
#include "shared/sim/entities/bullet.h"
#include "shared/sim/types.h"
#include <array>
#include <cstddef>
#include <mathlib.hpp>
#include <optional>

namespace shared::sim {
struct EnemySimState {
  mathlib::Vec3f position;
  EnemyType type;
  bool alive = false;
  float shooting_cooldown = 0;

  static constexpr auto SIZE = mathlib::Vec3f{32, 20, 20};

  static constexpr float SHOOTING_INTERVAL_MIN = 1.5f;
  static constexpr float SHOOTING_INTERVAL_MAX = 3.5f;

  /// Activates the first free slot in @p bullets as a new enemy-owned
  /// bullet fired downward from this enemy's current position.
  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;

  [[nodiscard]]
  mathlib::Cuboidf getHitBox() const;
};

/// Owns the whole enemy grid: entrance descent, side-to-side/step-down
/// movement, per-enemy shooting, and win/loss condition checks
/// (allEnemiesDefeated / reachedPlayer).
struct EnemiesPoolSimState {
  using GetSlotFn = mathlib::Vec3f (*)(EnemyCount, EnemyCount);
  using GetEntranceFn = LinearEntrance (*)(EnemyCount);

  static constexpr auto SPACING_X = 40.0f;
  static constexpr auto SPACING_Z = 40.0f;
  static constexpr auto PITCH_Z = EnemySimState::SIZE.z + SPACING_Z;
  static constexpr auto PITCH_X = EnemySimState::SIZE.x + SPACING_X;

  static constexpr std::array<std::pair<PlayerCount, EnemyCount>, MAX_PLAYERS_COOP>
      ROWS_PER_PLAYERS_COUNT = {
          {{MAX_PLAYERS_COOP, MAX_ENEMY_ROWS},
           {MAX_PLAYERS_COOP - 1, MAX_ENEMY_ROWS - 2},
           {MAX_PLAYERS_COOP - 2, MAX_ENEMY_ROWS - 3},
           {MAX_PLAYERS_COOP - 3, MAX_ENEMY_ROWS - 5}}
  };

  EnemyCount active_rows{};
  std::array<EnemySimState, MAX_ENEMY_ROWS * ENEMY_COLS> enemies{};
  mathlib::Vec3f offset{};
  GetSlotFn slot_fn;
  LinearEntrance entrance{};

  /// Resets the grid for a new match: marks the first `active_rows`
  /// rows (per @p playersCount, see ROWS_PER_PLAYERS_COUNT) alive and the
  /// rest dead, and resets formation position/direction.
  void init(
      GetEntranceFn entranceFn, PlayerCount playersCount, EnemiesPoolSimState::GetSlotFn getSlotFn
  );

  void stepEntrance(float dt);

  /// Advances one tick of the fight: side-to-side movement with a
  /// step-down on hitting a screen edge, and enemy shooting.
  void
  step(mathlib::Vec3f offsetDelta, float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool);

  /// @return true once every enemy in the grid has been marked dead.
  bool allEnemiesDefeated();

  bool isEntranceComplete();

  void placeEnemy(EnemySimState &enemy, std::size_t idx);

  // The box is through the center of the enemies at limits, it does not enclose
  // the whole formation
  std::optional<mathlib::Cuboidf> aliveExtents() const;
};

} // namespace shared::sim
