#include "entities/boss.h"
#include "constants.h"
#include "entities/bullet.h"
#include "entities/player.h"
#include "raymath.h"
#include <array>
#include <cstddef>

void Boss::loadTexture() { texture_ = LoadTexture("assets/boss.png"); }

void Boss::unload() { UnloadTexture(texture_); }

void Boss::draw() const {
  if (!active)
    return;

  if (texture_.id != 0) {
    float scale = 80.0f / texture_.width;
    Vector2 draw_pos = {position.x - (texture_.width * scale) / 2.0f,
                        position.y - (texture_.height * scale) / 2.0f};
    DrawTextureEx(texture_, draw_pos, 0.0f, scale, WHITE);
  } else {
    DrawRectangle((int)position.x - 40, (int)position.y - 20, 80, 40, RED);
  }

  const char *text = TextFormat("Boss Health: %d", health);
  DrawText(text, SCREEN_WIDTH - 10 - MeasureText(text, STATUS_FONT_SIZE),
           10 + STATUS_FONT_SIZE + Player::HEART_SIZE, STATUS_FONT_SIZE, WHITE);
}

void Boss::spawnBullets(float dt, std::array<Bullet, MAX_BULLETS> &bullets,
                        Vector2 player_position) {
  if (shooting_pattern == BossPattern::SPREAD_SHOT) {
    std::array<Bullet *, SPREAD_SHOT_BULLETS_COUNT> reserved_bullets{};
    int count = 0;

    for (auto &b : bullets) {
      if (count == SPREAD_SHOT_BULLETS_COUNT)
        break;

      if (!b.active) {
        reserved_bullets[count] = &b;
        count++;
      }
    }

    for (auto b : reserved_bullets) {
      if (b == nullptr)
        continue;
      b->type = BulletType::ENEMY;
      b->active = true;
      b->position = {position.x, position.y + HEIGHT / 2};
    }

    if (reserved_bullets[0] != nullptr) {
      reserved_bullets[0]->velocity = {0, BULLETS_SPEED};
    }

    if (reserved_bullets[1] != nullptr) {
      reserved_bullets[1]->velocity =
          Vector2Scale(Vector2Normalize({position.x - player_position.x,
                                         player_position.y - position.y}),
                       BULLETS_SPEED);
    }

    if (reserved_bullets[2] != nullptr) {
      reserved_bullets[2]->velocity =
          Vector2Scale(Vector2Normalize({player_position.x - position.x,
                                         player_position.y - position.y}),
                       BULLETS_SPEED);
    }

    shooting_pattern = BossPattern::SUCCESSIVE_SHOTS;
    pattern_switching_cooldown =
        PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
  } else {
    phase2_shooting_cooldown -= dt;

    if (phase2_shooting_cooldown <= 0) {
      for (auto &b : bullets) {
        if (!b.active) {
          b.type = BulletType::ENEMY;
          b.active = true;
          b.position = {position.x, position.y + HEIGHT / 2};
          b.velocity = {0, Bullet::SPEED};
          break;
        }
      }

      phase2_bullets_shot++;
      phase2_shooting_cooldown = PHASE2_SHOOTING_COOLDOWN;

      if (phase2_bullets_shot == SUCCESSIVE_SHOTS_BULLETS_COUNT) {
        shooting_pattern = BossPattern::SPREAD_SHOT;
        phase2_bullets_shot = 0;
        pattern_switching_cooldown =
            PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
        return;
      }
    }
  }
}
