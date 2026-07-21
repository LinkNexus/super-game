#pragma once

#include "shared/bullet_sim.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/messages.h"
#include <array>

namespace shared {
struct PlayerSimState {
  Vec2D position = {0, 0};
  float fire_cooldown = 0.0f;
  int lives = INITIAL_LIVES;
  int points = 0;

  static constexpr float SIZE = 20.0f;
  static constexpr float SPEED = 300.0f;
  static constexpr float HITBOX_SCALE = 0.80f;
  static constexpr float FIRE_COOLDOWN = 0.15f;
  static constexpr int INITIAL_LIVES = 3;

  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;
  void update(PlayerInput input, float dt,
              std::array<BulletSimState, MAX_BULLETS> &bullets,
              bool can_fire_bullets);
};
} // namespace shared
