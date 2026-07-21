#include "shared/enemy_sim.h"
#include "shared/bullet_sim.h"
#include "shared/constants.h"

void shared::EnemySimState::spawnBullet(
    std::array<shared::BulletSimState, MAX_BULLETS> &bullets) const {
  for (auto &bullet : bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.position = {position.x, position.y + HEIGHT / 2};
      bullet.type = shared::BulletType::ENEMY;
      break;
    }
  }
}
