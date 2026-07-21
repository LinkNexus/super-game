#include "entities/bullet.h"
#include "raylib.h"
#include "shared/bullet_sim.h"
#include "shared/messages.h"

void Bullet::draw(const shared::BulletState &state) const {
  if (!state.active)
    return;
  DrawRectangle((int)state.position.x - shared::BulletSimState::WIDTH / 2,
                (int)state.position.y - shared::BulletSimState::HEIGHT / 2,
                shared::BulletSimState::WIDTH, shared::BulletSimState::HEIGHT,
                YELLOW);
}
