#pragma once

#include "shared/sim/constants.h"
#include "shared/sim/entities/bullet.h"
#include "shared/sim/types.h"
#include <array>
#include <cstdint>
#include <mathlib.hpp>
#include <span>

namespace shared::sim {
/// The boss alternates between these two attack patterns; see
/// BossSimState::spawnBullets().
enum class BossPattern {
  /// A fan of bullets spread across the full screen width, evenly angled
  /// via Vec2D::rotated() from the boss's current position.
  SPREAD_SHOT,
  /// A burst of straight-down bullets fired in quick succession.
  SUCCESSIVE_SHOTS,
};

/// Full internal boss state, owned by GameSim. Health/bullet counts scale
/// with player count (see init()) so co-op stays challenging.
struct BossSimState {
  using BulletsSpreadFn = void (*)(mathlib::Vec3f from, std::span<mathlib::Vec3f> out);

  mathlib::Vec3f position;
  BossHealth health;
  /// Health at the start of the fight for the current player count; used
  /// both for phase-switch thresholds (as a fraction, see
  /// MIN_LP_PER_SWITCH) and for the client's health-bar fill ratio.
  BossHealth max_health;
  bool active;
  /// 1-indexed fight phase (1..PHASES_COUNT), advances as health drops
  /// below each MIN_LP_PER_SWITCH threshold.
  uint8_t current_phase;
  float pattern_switching_cooldown;
  BossPattern shooting_pattern;

  float phase2_shooting_cooldown;
  BulletCount phase2_bullets_shot;

  BulletCount spread_shot_bullets_count;
  BulletCount successive_shots_bullets_count;

  LinearEntrance entrance;

  static constexpr auto SIZE = mathlib::Vec3f{100, 100, 87};

  /// Base health for a single player; scaled by player count in init().
  static constexpr BossHealth INITIAL_LP = 100;

  static constexpr auto BULLETS_SPEED = BulletSimState::SPEED * 1.5f;

  static constexpr BulletCount MAX_SPREAD_SHOT_BULLETS = 16;
  static constexpr BulletCount MAX_SUCCESSIVE_SHOTS_BULLETS = 12;

  static constexpr auto PHASE2_SHOOTING_COOLDOWN = 0.3f;

  static constexpr uint8_t PHASES_COUNT = 3;
  /// Health fractions (of `max_health`) at which the fight advances to the
  /// next phase - fractions rather than absolute values so phase timing
  /// stays proportional regardless of player-count scaling.
  static constexpr std::array<float, PHASES_COUNT - 1> MIN_LP_PER_SWITCH = {0.75f, 0.50f};
  static constexpr std::array<float, PHASES_COUNT> PATTERN_SWITCHING_COOLDOWNS_PER_PHASE = {
      1.5f, 0.8f, 0.5f
  };

  /// Maps active player count to how many bullets a SPREAD_SHOT fires.
  /// Falls back to MAX_SPREAD_SHOT_BULLETS for any count not listed here.
  static constexpr std::array<std::pair<PlayerCount, BulletCount>, MAX_PLAYERS_COOP>
      SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT = {{
          {MAX_PLAYERS_COOP, MAX_SPREAD_SHOT_BULLETS},
          {MAX_PLAYERS_COOP - 1, MAX_SPREAD_SHOT_BULLETS - 2},
          {MAX_PLAYERS_COOP - 2, MAX_SPREAD_SHOT_BULLETS - 4},
          {MAX_PLAYERS_COOP - 3, MAX_SPREAD_SHOT_BULLETS - 6},
      }};
  /// Maps active player count to how many bullets a SUCCESSIVE_SHOTS burst
  /// fires. Falls back to MAX_SUCCESSIVE_SHOTS_BULLETS otherwise.
  static constexpr std::array<std::pair<PlayerCount, BulletCount>, MAX_PLAYERS_COOP>
      SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT = {{
          {MAX_PLAYERS_COOP, MAX_SUCCESSIVE_SHOTS_BULLETS},
          {MAX_PLAYERS_COOP - 1, MAX_SUCCESSIVE_SHOTS_BULLETS - 2},
          {MAX_PLAYERS_COOP - 2, MAX_SUCCESSIVE_SHOTS_BULLETS - 4},
          {MAX_PLAYERS_COOP - 3, MAX_SUCCESSIVE_SHOTS_BULLETS - 6},
      }};

  /// Fires the current `shooting_pattern` into @p bullets once
  /// `pattern_switching_cooldown` has elapsed, then switches to the other
  /// pattern (SPREAD_SHOT) or continues the burst (SUCCESSIVE_SHOTS).
  void spawnBullets(
      BossSimState::BulletsSpreadFn spreadFn,
      float dt,
      const std::array<std::optional<mathlib::Vec3f>, MAX_PLAYERS_COOP> &playersPositions,
      std::array<BulletSimState, MAX_BULLETS> &bullets
  );

  /// Resets the boss for a new fight, scaling health and bullet counts by
  /// @p playersCount.
  void init(LinearEntrance entrance, PlayerCount playersCount);

  /// Advances one tick of the fight: side-to-side movement, phase
  /// progression based on remaining health, and attack pattern firing.
  void step(
      BossSimState::BulletsSpreadFn spreadFn,
      const mathlib::Vec3f &newPosition,
      const std::array<std::optional<mathlib::Vec3f>, MAX_PLAYERS_COOP> &playersPositions,
      float dt,
      std::array<BulletSimState, MAX_BULLETS> &bullets
  );

  [[nodiscard]]
  mathlib::Cuboidf getHitBox() const;

  void stepEntrance(float dt);

  bool isEntranceComplete() const;
};
} // namespace shared::sim
