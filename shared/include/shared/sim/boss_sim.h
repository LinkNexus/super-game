#pragma once

#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/sim/bullet_sim.h"
#include <array>
#include <cstdint>

namespace shared {

enum class BossPattern {
  SPREAD_SHOT,
  SUCCESSIVE_SHOTS,
};

struct BossSimState {
  Vec2D position;
  int32_t health;
  bool active;
  int8_t current_phase;
  float pattern_switching_cooldown;
  BossPattern shooting_pattern;

  float phase2_shooting_cooldown;
  int phase2_bullets_shot;
  int direction;

  static constexpr float WIDTH = 100.0f;
  static constexpr float HEIGHT = 87.0f;

  static constexpr int INITIAL_LP = 100;
  static constexpr float INITIAL_POSITION_Y = -30.0f;
  static constexpr float FINAL_POSITION_Y = 80.0f;
  static constexpr float INITIAL_DESCENT_SPEED = 70.0f;
  static constexpr float CYCLE_SPEED = 400.0f;

  static constexpr float BULLETS_SPEED = BulletSimState::SPEED * 1.5f;

  static constexpr int SPREAD_SHOT_BULLETS_COUNT = 3;
  static constexpr int SUCCESSIVE_SHOTS_BULLETS_COUNT = 5;

  static constexpr float PHASE2_SHOOTING_COOLDOWN = 0.3f;

  static constexpr int PHASES_COUNT = 3;
  static constexpr std::array<int, PHASES_COUNT - 1> MIN_LP_PER_SWITCH = {75,
                                                                          50};
  static constexpr std::array<float, PHASES_COUNT>
      PATTERN_SWITCHING_COOLDOWNS_PER_PHASE = {1.5f, 0.8f, 0.5f};

  void
  spawnBullets(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets,
               std::array<std::optional<Vec2D>, MAX_PLAYERS> &player_positions);
  void init();
  void stepEntrance(float dt);
  bool isEntranceComplete();
  void step(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool,
            std::array<std::optional<Vec2D>, MAX_PLAYERS> &player_positions);
};
} // namespace shared
