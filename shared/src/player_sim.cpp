#include "shared/player_sim.h"
#include "shared/bullet_sim.h"
#include "shared/constants.h"
#include "shared/messages.h"

void shared::PlayerSimState::spawnBullet(
    std::array<BulletSimState, MAX_BULLETS> &bullets) const {
  for (auto &bullet : bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.position = {position.x, position.y - SIZE};
      bullet.velocity = {0.0f, -BulletSimState::SPEED};
      return;
    }
  }
}

void shared::PlayerSimState::update(
    shared::PlayerInput input, float dt,
    std::array<BulletSimState, MAX_BULLETS> &bullets, bool can_fire_bullets) {
  float vx = 0;

  if (input.buttons & shared::BUTTON_LEFT)
    vx = -1.0f;
  if (input.buttons & shared::BUTTON_RIGHT)
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

  if (can_fire_bullets && (input.buttons & shared::BUTTON_FIRE) &&
      fire_cooldown <= 0.0f) {
    spawnBullet(bullets);
    fire_cooldown = FIRE_COOLDOWN;
  }
}
