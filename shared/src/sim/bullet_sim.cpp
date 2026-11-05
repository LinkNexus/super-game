#include "shared/sim/bullet_sim.h"
#include "shared/constants.h"
#include <mathlib.hpp>

void shared::BulletSimState::step(float dt) {
  if (!active)
    return;

  position += velocity * dt;

  if (position.y < 0 || position.y > SCREEN_HEIGHT)
    active = false;
}

mathlib::Rectanglef shared::BulletSimState::getHitBox() const {
  return mathlib::Rectanglef::fromCenterAndSize(position, WIDTH, HEIGHT);
}
