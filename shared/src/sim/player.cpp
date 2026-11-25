#include "shared/sim/entities/player.h"
#include "shared/sim/constants.h"
#include "shared/sim/entities/bullet.h"
#include "shared/sim/types.h"
#include <mathlib.hpp>

void shared::sim::PlayerSimState::init(
    PlayerId id, LinearEntrance entrance, int lives, float orientation
) {
  this->entrance = entrance;
  position = entrance.trajectory.start;
  fire_cooldown = 0.0f;
  this->lives = lives;
  this->orientation = orientation;
  points = 0;
  this->id = id;
}

void shared::sim::PlayerSimState::spawnBullet(
    std::array<BulletSimState, MAX_BULLETS> &bullets
) const {
  for (auto &bullet : bullets) {
    if (!bullet.active) {
      bullet.active = true;

      const auto forward =
          mathlib::Vec3f::unitZ().rotatedAroundAxis(-mathlib::Vec3f::unitY(), orientation);

      bullet.position = position + forward.scaledBy(PlayerSimState::SIZE / 2);
      bullet.velocity = (forward * BulletSimState::SPEED);

      bullet.owner_id = id;
      bullet.type = BulletType::PLAYER;
      return;
    }
  }
}

void shared::sim::PlayerSimState::stepEntrance(float dt) {
  position.moveTowards(entrance.trajectory.end, entrance.speed * dt);
}

bool shared::sim::PlayerSimState::isEntranceComplete() const {
  return position.approxEquals(entrance.trajectory.end);
}

void shared::sim::PlayerSimState::step(
    const mathlib::Vec3f &direction,
    const mathlib::Cuboidf &bounds,
    float dt,
    std::array<BulletSimState, MAX_BULLETS> &bullets,
    bool isShooting,
    bool canShoot
) {
  position += direction * SPEED * dt;

  if (!bounds.containsPoint(position)) {
    position = bounds.closestPointTo(position);
  }

  if (fire_cooldown > 0.0f)
    fire_cooldown -= dt;

  if (canShoot && isShooting && fire_cooldown <= 0.0f) {
    spawnBullet(bullets);
    fire_cooldown = FIRE_COOLDOWN;
  }
}

mathlib::Cuboidf shared::sim::PlayerSimState::getHitBox() const {
  return mathlib::Cuboidf::fromCenterAndSize(position, SIZE * HITBOX_SCALE);
}
