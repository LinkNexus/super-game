#pragma once

#include "shared/bullet_sim.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include <array>
#include <cstdint>

namespace shared {

enum class BossPattern {
  SPREAD_SHOT,
  SUCCESSIVE_SHOTS,
};

struct BossSimState {
  Vec2D position = {0.0f, 0.0f};
  int32_t health = INITIAL_LP;
  bool active = false;
  int8_t current_phase = 1;
  float pattern_switching_cooldown =
      PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
  BossPattern shooting_pattern = BossPattern::SPREAD_SHOT;

  float phase2_shooting_cooldown = PHASE2_SHOOTING_COOLDOWN;
  int phase2_bullets_shot = 0;

  static constexpr float WIDTH = 100.0f;
  static constexpr float HEIGHT = 87.0f;

  static constexpr int INITIAL_LP = 100;

  static constexpr float BULLETS_SPEED = BulletSimState::SPEED * 1.5f;

  static constexpr int SPREAD_SHOT_BULLETS_COUNT = 3;
  static constexpr int SUCCESSIVE_SHOTS_BULLETS_COUNT = 5;

  static constexpr float PHASE2_SHOOTING_COOLDOWN = 0.3f;

  static constexpr int PHASES_COUNT = 3;
  static constexpr std::array<int, PHASES_COUNT - 1> MIN_LP_PER_SWITCH = {75,
                                                                          50};
  static constexpr std::array<float, PHASES_COUNT>
      PATTERN_SWITCHING_COOLDOWNS_PER_PHASE = {1.5f, 0.8f, 0.5f};

  void spawnBullets(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets,
                    Vec2D player_position);
};
} // namespace shared
