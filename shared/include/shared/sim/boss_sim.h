#pragma once

#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/sim/bullet_sim.h"
#include <array>
#include <cstdint>

namespace shared {

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
  Vec2D position;
  int32_t health;
  /// Health at the start of the fight for the current player count; used
  /// both for phase-switch thresholds (as a fraction, see
  /// MIN_LP_PER_SWITCH) and for the client's health-bar fill ratio.
  int32_t max_health;
  bool active;
  /// 1-indexed fight phase (1..PHASES_COUNT), advances as health drops
  /// below each MIN_LP_PER_SWITCH threshold.
  int8_t current_phase;
  float pattern_switching_cooldown;
  BossPattern shooting_pattern;

  float phase2_shooting_cooldown;
  int phase2_bullets_shot;

  int spread_shot_bullets_count;
  int successive_shots_bullets_count;

  /// Horizontal movement direction: +1 or -1.
  int direction;

  static constexpr float WIDTH = 100.0f;
  static constexpr float HEIGHT = 87.0f;

  /// Base health for a single player; scaled by player count in init().
  static constexpr int INITIAL_LP = 100;
  static constexpr float INITIAL_POSITION_Y = -30.0f;
  static constexpr float FINAL_POSITION_Y = 80.0f;
  static constexpr float INITIAL_DESCENT_SPEED = 70.0f;
  static constexpr float CYCLE_SPEED = 400.0f;

  static constexpr float BULLETS_SPEED = BulletSimState::SPEED * 1.5f;

  static constexpr int MAX_SPREAD_SHOT_BULLETS = 10;
  static constexpr int MAX_SUCCESSIVE_SHOTS_BULLETS = 8;

  static constexpr float PHASE2_SHOOTING_COOLDOWN = 0.3f;

  static constexpr int PHASES_COUNT = 3;
  /// Health fractions (of `max_health`) at which the fight advances to the
  /// next phase - fractions rather than absolute values so phase timing
  /// stays proportional regardless of player-count scaling.
  static constexpr std::array<float, PHASES_COUNT - 1> MIN_LP_PER_SWITCH = {
      0.75f, 0.50f};
  static constexpr std::array<float, PHASES_COUNT>
      PATTERN_SWITCHING_COOLDOWNS_PER_PHASE = {1.5f, 0.8f, 0.5f};

  /// Maps active player count to how many bullets a SPREAD_SHOT fires.
  /// Falls back to MAX_SPREAD_SHOT_BULLETS for any count not listed here.
  static constexpr std::array<std::pair<uint8_t, int>, MAX_PLAYERS>
      SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT = {{
          {MAX_PLAYERS, MAX_SPREAD_SHOT_BULLETS},
          {MAX_PLAYERS - 1, MAX_SPREAD_SHOT_BULLETS - 2},
      }};
  /// Maps active player count to how many bullets a SUCCESSIVE_SHOTS burst
  /// fires. Falls back to MAX_SUCCESSIVE_SHOTS_BULLETS otherwise.
  static constexpr std::array<std::pair<uint8_t, int>, MAX_PLAYERS>
      SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT = {{
          {MAX_PLAYERS, MAX_SUCCESSIVE_SHOTS_BULLETS},
          {MAX_PLAYERS - 1, MAX_SUCCESSIVE_SHOTS_BULLETS - 2},
      }};

  /// Fires the current `shooting_pattern` into @p bullets once
  /// `pattern_switching_cooldown` has elapsed, then switches to the other
  /// pattern (SPREAD_SHOT) or continues the burst (SUCCESSIVE_SHOTS).
  void spawnBullets(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets);

  /// Resets the boss for a new fight, scaling health and bullet counts by
  /// @p playersCount.
  void init(uint8_t playersCount);

  /// Advances the entrance descent from off-screen to `FINAL_POSITION_Y`.
  void stepEntrance(float dt);

  /// @return true once the entrance descent has reached `FINAL_POSITION_Y`.
  bool isEntranceComplete();

  /// Advances one tick of the fight: side-to-side movement, phase
  /// progression based on remaining health, and attack pattern firing.
  void step(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool);
};
} // namespace shared
