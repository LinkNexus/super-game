#include "shared/sim/entities/bullet.h"
#include <mathlib.hpp>

void shared::sim::BulletSimState::step(const mathlib::Cuboidf &bounds,
                                       float dt) {
  if (!active)
    return;

  position += velocity * dt;

  if (!bounds.containsPoint(position))
    active = false;
}

mathlib::Cuboidf shared::sim::BulletSimState::getHitBox() const {
  return mathlib::Cuboidf::fromCenterAndSize(position, SIZE);
}
