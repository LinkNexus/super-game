#include "entities/player.h"
#include "constants.h"
#include "entities/bullet.h"

void Player::update(float dt, std::array<Bullet, MAX_BULLETS> &bullets,
                    bool can_fire_bullets) {
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

  if (can_fire_bullets && IsKeyDown(KEY_SPACE) && fire_cooldown <= 0.0f) {
    spawnBullet(bullets);
    fire_cooldown = FIRE_COOLDOWN;
  }
}

void Player::loadTexture() { texture = LoadTexture("assets/playerShip.png"); }

void Player::unload() { UnloadTexture(texture); }

void Player::draw() const {
  if (texture.id != 0) {
    float scale = (SIZE * 2.0f) / texture.width;
    Vector2 draw_pos = {position.x - (texture.width * scale) / 2.0f,
                        position.y - (texture.height * scale) / 2.0f};
    DrawTextureEx(texture, draw_pos, 0.0f, scale, WHITE);
  } else {
    Vector2 tip = {position.x, position.y - SIZE};
    Vector2 left = {position.x - SIZE * 0.7f, position.y + SIZE * 0.7f};
    Vector2 right = {position.x + SIZE * 0.7f, position.y + SIZE * 0.7f};
    DrawTriangle(tip, left, right, SKYBLUE);
    DrawTriangleLines(tip, left, right, WHITE);
  }
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
