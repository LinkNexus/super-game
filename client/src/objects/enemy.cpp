#include "objects/enemy.h"
#include "objects/bullet.h"

void Enemy::draw() const {
  if (!alive)
    return;
  Color c = (type == EnemyType::TYPE_1) ? GREEN : ORANGE;
  DrawRectangle((int)position.x - WIDTH / 2, (int)position.y - HEIGHT / 2,
                WIDTH, HEIGHT, c);
}

void Enemy::spawnBullet(std::array<Bullet, MAX_BULLETS> &bullets) const {
  for (auto &b : bullets) {
    if (!b.active) {
      b.position = {position.x, position.y + HEIGHT / 2};
      b.velocity = {0.0f, Bullet::SPEED};
      b.active = true;
      b.type = BulletType::ENEMY;
      return;
    }
  }
}
