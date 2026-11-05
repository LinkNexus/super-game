#pragma once

#include "shared/constants.h"
#include "shared/types.h"
#include <limits>
#include <mathlib.hpp>

namespace shared {
/// One slot in the fixed-size bullet pool (`MAX_BULLETS` per GameSim).
/// Inactive slots are reused rather than the pool being resized.
struct BulletSimState {
  mathlib::Vec2f position{};
  mathlib::Vec2f velocity{};
  BulletType type{};
  PlayerId owner_id = std::numeric_limits<PlayerId>::max();
  bool active = false;

  static constexpr float SPEED = 600.0f;
  static constexpr float WIDTH = 4;
  static constexpr float HEIGHT = 12;

  /// Advances position by `velocity * dt` and deactivates the bullet once
  /// it crosses the top or bottom of the screen.
  void step(float dt);

  [[nodiscard]]
  mathlib::Rectanglef getHitBox() const;
};

using BulletsPool = std::array<BulletSimState, MAX_BULLETS>;
} // namespace shared
