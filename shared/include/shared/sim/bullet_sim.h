#pragma once

#include "shared/math_utils.h"
#include <cstdint>

namespace shared {
/// Distinguishes player-fired bullets (damage enemies/boss) from
/// enemy/boss-fired bullets (damage players) for collision handling.
enum class BulletType : uint8_t { ENEMY, PLAYER };

/// One slot in the fixed-size bullet pool (`MAX_BULLETS` per GameSim).
/// Inactive slots are reused rather than the pool being resized.
struct BulletSimState {
  Vec2D position;
  Vec2D velocity;
  BulletType type;
  /// id of the player who fired this bullet, or `UINT8_MAX` for
  /// enemy/boss-owned bullets. Used to credit points on a hit.
  uint8_t owner_id = UINT8_MAX;
  bool active = false;

  static constexpr float SPEED = 600.0f;
  static constexpr float WIDTH = 4;
  static constexpr float HEIGHT = 12;

  /// Advances position by `velocity * dt` and deactivates the bullet once
  /// it crosses the top or bottom of the screen.
  void step(float dt);
};

} // namespace shared
