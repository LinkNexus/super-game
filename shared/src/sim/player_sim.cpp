#include "shared/sim/player_sim.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/bullet_sim.h"

using namespace shared;

void PlayerSimState::init(uint8_t id) {
  position = {INITIAL_POSITION_X, POSITION_Y};
  fire_cooldown = 0.0f;
  lives = INITIAL_LIVES;
  points = 0;
  this->id = id;
}

void PlayerSimState::spawnBullet(
    std::array<BulletSimState, MAX_BULLETS> &bullets) const {
  for (auto &bullet : bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.position = {position.x, position.y - SIZE};
      bullet.velocity = {0.0f, -BulletSimState::SPEED};
      bullet.owner_id = id;
      bullet.type = BulletType::PLAYER;
      return;
    }
  }
}

void PlayerSimState::step(const PlayerInput &input, float dt,
                          std::array<BulletSimState, MAX_BULLETS> &bullets,
                          bool can_fire_bullets) {
  float vx = 0;

  if (input.buttons & BUTTON_LEFT)
    vx = -1.0f;
  if (input.buttons & BUTTON_RIGHT)
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

  if (can_fire_bullets && (input.buttons & BUTTON_SHOOT) &&
      fire_cooldown <= 0.0f) {
    spawnBullet(bullets);
    fire_cooldown = FIRE_COOLDOWN;
  }
}
