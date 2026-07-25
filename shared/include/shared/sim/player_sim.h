#pragma once

#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/messages.h"
#include "shared/sim/bullet_sim.h"
#include <array>

namespace shared {
struct PlayerSimState {
  Vec2D position;
  float fire_cooldown;
  int lives;
  int points;
  uint8_t id;

  static constexpr float SIZE = 20.0f;
  static constexpr float SPEED = 300.0f;
  static constexpr float HITBOX_SCALE = 0.80f;
  static constexpr float FIRE_COOLDOWN = 0.15f;
  static constexpr int INITIAL_LIVES = 3;
  static constexpr float POSITION_Y = SCREEN_HEIGHT - 60.0f;
  static constexpr float INITIAL_POSITION_X = SCREEN_WIDTH / 2.0f;

  void init(uint8_t id);
  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;
  void step(const PlayerInput &input, float dt,
            std::array<BulletSimState, MAX_BULLETS> &bullets,
            bool can_fire_bullets);
};
} // namespace shared
