#include "objects/player.h"
#include "constants.h"
#include "objects/bullet.h"
#include <array>

void Player::update(float dt, std::array<Bullet, MAX_BULLETS> &bullets) {
  float vx = 0;
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
    vx = -1.0f;
  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
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

  if (IsKeyDown(KEY_SPACE) && fire_cooldown <= 0.0f) {
    spawnBullet(bullets);
    fire_cooldown = FIRE_COOLDOWN;
  }
}

void Player::draw() const {
  Vector2 tip = {position.x, position.y - SIZE};
  Vector2 left = {position.x - SIZE * 0.7f, position.y + SIZE * 0.7f};
  Vector2 right = {position.x + SIZE * 0.7f, position.y + SIZE * 0.7f};

  DrawTriangle(tip, left, right, SKYBLUE);
  DrawTriangleLines(tip, left, right, WHITE);
}

void Player::spawnBullet(std::array<Bullet, MAX_BULLETS> &bullets) const {
  for (auto &b : bullets) {
    if (!b.active) {
      b.position = {position.x, position.y - SIZE};
      b.velocity = {0.0f, -Bullet::SPEED};
      b.active = true;
      b.type = BulletType::PLAYER;
      return;
    }
  }
}
