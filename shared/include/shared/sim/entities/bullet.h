#pragma once

#include "shared/sim/constants.h"
#include "shared/sim/types.h"
#include <limits>
#include <mathlib.hpp>

namespace shared::sim {
struct BulletSimState {
  mathlib::Vec3f position{};
  mathlib::Vec3f velocity{};
  BulletType type{};
  PlayerId owner_id = std::numeric_limits<PlayerId>::max();
  bool active = false;

  static constexpr float SPEED = 600.0f;
  static constexpr auto SIZE = mathlib::Vec3f{4, 4, 12};

  void step(const mathlib::Cuboidf &bounds, float dt);

  [[nodiscard]]
  mathlib::Cuboidf getHitBox() const;
};

using BulletsPool = std::array<BulletSimState, MAX_BULLETS>;
} // namespace shared::sim
