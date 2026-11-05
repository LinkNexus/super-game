#include "shared/sim/player_sim.h"
#include "shared/constants.h"
#include "shared/sim/bullet_sim.h"
#include <mathlib.hpp>

using namespace shared;

void PlayerSimState::init(PlayerId id, mathlib::Vec2f position, int lives,
                          float orientation) {
  this->position = position;
  fire_cooldown = 0.0f;
  this->lives = lives;
  this->orientation = orientation;
  points = 0;
  this->id = id;
}

void PlayerSimState::spawnBullet(
    std::array<BulletSimState, MAX_BULLETS> &bullets) const {
  for (auto &bullet : bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.position = position + mathlib::Vec2f{0, SIZE}.rotated(orientation);

      bullet.velocity =
          mathlib::Vec2f{0.0f, BulletSimState::SPEED}.rotated(orientation);
      bullet.owner_id = id;
      bullet.type = BulletType::PLAYER;
      return;
    }
  }
}

void PlayerSimState::step(Button buttons, float dt,
                          std::array<BulletSimState, MAX_BULLETS> &bullets,
                          bool can_fire_bullets) {
  float vx = 0;

  if (buttons & BUTTON_LEFT)
    vx = -1.0f;
  if (buttons & BUTTON_RIGHT)
    vx = 1.0f;

  position.x += vx * SPEED * dt;

  if (position.x < SIZE)
    position.x = SIZE;
  if (position.x > SCREEN_WIDTH - SIZE)
    position.x = SCREEN_WIDTH - SIZE;
  if (position.y < SIZE)
    position.y = SIZE;
  if (position.y > SCREEN_HEIGHT - SIZE)
    position.y = SCREEN_HEIGHT - SIZE;

  if (fire_cooldown > 0.0f)
    fire_cooldown -= dt;

  if (can_fire_bullets && (buttons & BUTTON_SHOOT) && fire_cooldown <= 0.0f) {
    spawnBullet(bullets);
    fire_cooldown = FIRE_COOLDOWN;
  }
}

mathlib::Rectanglef shared::PlayerSimState::getHitBox() const {
  return mathlib::Rectanglef::fromCenterAndSize(position, SIZE, SIZE)
      .scaledFromCenterBy(HITBOX_SCALE);
}
