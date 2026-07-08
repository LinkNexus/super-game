#pragma once
#include "constants.h"
#include "objects/bullet.h"
#include "raylib.h"
#include <array>

struct Boss {
  Vector2 position = {0, 0};
  int health = 200;
  bool active = false;

  float pattern_switching_cooldown = PATTERN_SWITCHING_COOLDOWN;
  int current_phase = 1;
  int shooting_pattern = -1;

  float phase2_shooting_cooldown = PHASE2_SHOOTING_COOLDOWN;
  int phase2_bullets_shooted = 0;

  static constexpr float WIDTH = 100.0f;
  static constexpr float HEIGHT = 60.0f;

  static constexpr float PATTERN_SWITCHING_COOLDOWN = 1.5f;
  static constexpr float PHASE2_SHOOTING_COOLDOWN = 0.3f;

  static constexpr int PHASES_COUNT = 3;
  static constexpr std::array<int, PHASES_COUNT - 1> min_lp_per_switch = {75,
                                                                          50};
  static constexpr std::array<float, PHASES_COUNT> phases_cooldowns = {
      1.5f, 0.8f, 0.5f};

  void draw() const;
  void spawnBullets(float dt, std::array<Bullet, MAX_BULLETS> &bullets,
                    Vector2 player_position);
};
