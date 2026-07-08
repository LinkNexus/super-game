#include "objects/boss.h"
#include "constants.h"
#include "objects/bullet.h"
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
  if (shooting_pattern == -1) {
    std::array<Bullet *, 3> reserved_bullets;
    int count = 0;

    for (auto &b : bullets) {
      if (count == 3)
        break;

      if (!b.active) {
        reserved_bullets[count] = &b;
        count++;
      }
    }

    for (auto b : reserved_bullets) {
      b->type = BulletType::ENEMY;
      b->active = true;
      b->position = {position.x, position.y + HEIGHT / 2};
    }

    reserved_bullets[0]->velocity = {0, Bullet::SPEED * 1.5f};
    reserved_bullets[1]->velocity =
        Vector2Scale(Vector2Normalize({position.x - player_position.x,
                                       player_position.y - position.y}),
                     Bullet::SPEED * 1.5f);
    reserved_bullets[2]->velocity =
        Vector2Scale(Vector2Normalize({player_position.x - position.x,
                                       player_position.y - position.y}),
                     Bullet::SPEED * 1.5f);

    shooting_pattern *= -1;
    pattern_switching_cooldown = PATTERN_SWITCHING_COOLDOWN;
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

      phase2_bullets_shooted++;
      phase2_shooting_cooldown = PHASE2_SHOOTING_COOLDOWN;

      if (phase2_bullets_shooted == 5) {
        shooting_pattern *= -1;
        phase2_bullets_shooted = 0;
        pattern_switching_cooldown = PATTERN_SWITCHING_COOLDOWN;
        return;
      }
    }
  }
}
