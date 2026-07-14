#pragma once
#include "constants.h"
#include "entities/bullet.h"
#include "raylib.h"
#include <array>

enum class BossPattern {
  SPREAD_SHOT,
  SUCCESSIVE_SHOTS,
};

struct Boss {
  Vector2 position = {0, 0};
  int health = 100;
  bool active = false;

  int current_phase = 1;
  float pattern_switching_cooldown =
      PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
  BossPattern shooting_pattern = BossPattern::SPREAD_SHOT;

  float phase2_shooting_cooldown = PHASE2_SHOOTING_COOLDOWN;
  int phase2_bullets_shot = 0;

  static constexpr float WIDTH = 100.0f;
  static constexpr float HEIGHT = 60.0f;

  static constexpr float BULLETS_SPEED = Bullet::SPEED * 1.5f;

  static constexpr int SPREAD_SHOT_BULLETS_COUNT = 3;
  static constexpr int SUCCESSIVE_SHOTS_BULLETS_COUNT = 5;

  static constexpr float PHASE2_SHOOTING_COOLDOWN = 0.3f;

  static constexpr int PHASES_COUNT = 3;
  static constexpr std::array<int, PHASES_COUNT - 1> MIN_LP_PER_SWITCH = {75,
                                                                          50};
  static constexpr std::array<float, PHASES_COUNT>
      PATTERN_SWITCHING_COOLDOWNS_PER_PHASE = {1.5f, 0.8f, 0.5f};

  void draw() const;
  void spawnBullets(float dt, std::array<Bullet, MAX_BULLETS> &bullets,
                    Vector2 player_position);
  void loadTexture();
  void unload();

private:
  Texture2D texture_ = {};
};
