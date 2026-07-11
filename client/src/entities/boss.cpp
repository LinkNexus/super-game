#include "entities/boss.h"
#include "constants.h"
#include "entities/bullet.h"
#include "raymath.h"
#include <array>

void Boss::draw() const {
  if (!active)
    return;
  DrawRectangle((int)position.x - WIDTH / 2, (int)position.y - HEIGHT / 2,
                WIDTH, HEIGHT, RED);
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

    reserved_bullets[0]->velocity = {0, BULLETS_SPEED};

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
