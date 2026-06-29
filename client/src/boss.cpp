#include "objects/boss.h"

void Boss::draw() const {
  if (!active) return;
  DrawRectangle((int)position.x - 40, (int)position.y - 20, 80, 40, RED);
}
