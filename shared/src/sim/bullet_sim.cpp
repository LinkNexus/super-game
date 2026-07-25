#include "shared/sim/bullet_sim.h"
#include "shared/constants.h"

void shared::BulletSimState::step(float dt) {
  if (!active)
    return;

  position += velocity * dt;

  if (position.y < 0 || position.y > SCREEN_HEIGHT)
    active = false;
}
