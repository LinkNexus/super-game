#include "entities/bullet.h"
#include "constants.h"

void Bullet::update(float dt) {
  if (!active)
    return;
  position.x += velocity.x * dt;
  position.y += velocity.y * dt;

  if (position.y < 0 || position.y > SCREEN_HEIGHT)
    active = false;
}

void Bullet::draw() const {
  if (!active)
    return;
  DrawRectangle((int)position.x - WIDTH / 2, (int)position.y - HEIGHT / 2,
                WIDTH, HEIGHT, YELLOW);
}
