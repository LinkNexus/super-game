#pragma once

#include "shared/math_utils.h"
#include <cstdint>

namespace shared {
enum class BulletType : uint8_t { ENEMY, PLAYER };

struct BulletSimState {
  Vec2D position;
  Vec2D velocity;
  BulletType type;
  uint8_t owner_id = UINT8_MAX;
  bool active = false;

  static constexpr float SPEED = 600.0f;
  static constexpr float WIDTH = 4;
  static constexpr float HEIGHT = 12;

  void step(float dt);
};

} // namespace shared
