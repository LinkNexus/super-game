#include "objects/bullet.h"
#include "constants.h"

void Bullet::update(float dt) {
  if (!active) return;
  position.x += velocity.x * dt;
  position.y += velocity.y * dt;

  if (position.y < 0 || position.y > SCREEN_HEIGHT)
    active = false;
}

void Bullet::draw() const {
  if (!active) return;
  DrawRectangle((int)position.x - 2, (int)position.y - 6, 4, 12, YELLOW);
}
