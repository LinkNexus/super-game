#include "objects/enemy.h"

void Enemy::draw() const {
  if (!alive) return;
  Color c = (type == 0) ? GREEN : ORANGE;
  DrawRectangle((int)position.x - 16, (int)position.y - 10, 32, 20, c);
}
