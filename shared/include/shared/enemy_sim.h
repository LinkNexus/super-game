#pragma once

#include "shared/bullet_sim.h"
#include "shared/constants.h"
#include <array>
#include <cstdint>

namespace shared {
enum class EnemyType : uint8_t {
  TYPE_1,
  TYPE_2,
};

struct EnemySimState {
  Vec2D position;
  EnemyType type;
  bool alive = false;
  float shooting_cooldown = 0;

  static constexpr float WIDTH = 32.0f;
  static constexpr float HEIGHT = 20.0f;
  static constexpr float CYCLE_SPEED = 200.0f;
  static constexpr float DESCENT_SPEED = 35.0f;
  static constexpr float SHOOTING_INTERVAL_MIN = 1.5f;
  static constexpr float SHOOTING_INTERVAL_MAX = 3.5f;

  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;
};

} // namespace shared
