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
  int32_t max_health;
  bool active;
  int8_t current_phase;
  float pattern_switching_cooldown;
  BossPattern shooting_pattern;

  float phase2_shooting_cooldown;
  int phase2_bullets_shot;

  int spread_shot_bullets_count;
  int successive_shots_bullets_count;

  int direction;

  static constexpr float WIDTH = 100.0f;
  static constexpr float HEIGHT = 87.0f;

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
  static constexpr std::array<float, PHASES_COUNT - 1> MIN_LP_PER_SWITCH = {
      0.75f, 0.50f};
  static constexpr std::array<float, PHASES_COUNT>
      PATTERN_SWITCHING_COOLDOWNS_PER_PHASE = {1.5f, 0.8f, 0.5f};

  static constexpr std::array<std::pair<uint8_t, int>, MAX_PLAYERS>
      SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT = {{
          {MAX_PLAYERS, MAX_SPREAD_SHOT_BULLETS},
          {MAX_PLAYERS - 1, MAX_SPREAD_SHOT_BULLETS - 2},
      }};
  static constexpr std::array<std::pair<uint8_t, int>, MAX_PLAYERS>
      SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT = {{
          {MAX_PLAYERS, MAX_SUCCESSIVE_SHOTS_BULLETS},
          {MAX_PLAYERS - 1, MAX_SUCCESSIVE_SHOTS_BULLETS - 2},
      }};

  void spawnBullets(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets);
  void init(uint8_t playersCount);
  void stepEntrance(float dt);
  bool isEntranceComplete();
  void step(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool);
};
} // namespace shared
