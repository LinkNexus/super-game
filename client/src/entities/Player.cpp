#include "entities/player.h"
#include "constants.h"
#include "entities/bullet.h"
#include "raylib.h"

Texture2D Player::heart_texture_ = {};

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

void Player::loadTexture() {
   texture = LoadTexture("assets/playerShip.png");

  heart_texture_ = LoadTexture("assets/heart.png");
   }

void Player::unload() {
  UnloadTexture(texture);
  if (heart_texture_.id != 0) {
    UnloadTexture(heart_texture_);
  }
}

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

   for (int i = 0; i < lives; i++) {
    float x = SCREEN_WIDTH - 10 - HEART_SIZE - i * (HEART_SIZE + HEART_SPACING);
    float y = 10.0f;

    if (heart_texture_.id != 0) {
      float scale = HEART_SIZE / heart_texture_.width;
      DrawTextureEx(heart_texture_, {x, y}, 0.0f, scale, WHITE);
    } else {
      float cx = x + HEART_SIZE / 2.0f;
      float lobe_r = HEART_SIZE * 0.28f;
      float lobe_y = y + lobe_r;
      DrawCircle(cx - lobe_r, lobe_y, lobe_r, RED);
      DrawCircle(cx + lobe_r, lobe_y, lobe_r, RED);
      Vector2 bottom = {cx, y + HEART_SIZE};
      Vector2 leftPt = {x, lobe_y};
      Vector2 rightPt = {x + HEART_SIZE, lobe_y};
      DrawTriangle(leftPt, bottom, rightPt, RED);
    }
  }

  const char *points_text = TextFormat("Points: %d", points);
  DrawText(points_text,
           SCREEN_WIDTH - 10 - MeasureText(points_text, STATUS_FONT_SIZE),
           10 + STATUS_FONT_SIZE, STATUS_FONT_SIZE, WHITE);
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
