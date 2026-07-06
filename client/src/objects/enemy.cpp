#include "objects/enemy.h"

void Enemy::draw() const {
  if (!alive)
    return;
  Color c = (type == ENEMY_TYPE_1) ? GREEN : ORANGE;
  DrawRectangle((int)position.x - WIDTH / 2, (int)position.y - HEIGHT / 2,
                WIDTH, HEIGHT, c);
}
