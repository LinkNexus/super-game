#include "shared/boss_sim.h"

void shared::BossSimState::spawnBullets(
    float dt, std::array<shared::BulletSimState, MAX_BULLETS> &bullets,
    Vec2D player_position) {

  switch (shooting_pattern) {
  case BossPattern::SPREAD_SHOT: {
    std::array<shared::BulletSimState *, SPREAD_SHOT_BULLETS_COUNT>
        reserved_bullets{};
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
      b->active = true;
      b->position = {position.x, position.y + HEIGHT / 2};
    }

    if (reserved_bullets[0] != nullptr) {
      reserved_bullets[0]->velocity = {0, BULLETS_SPEED};
    }

    if (reserved_bullets[1] != nullptr) {
      reserved_bullets[1]->velocity =
          (position - player_position).rotated(90).normalized() * BULLETS_SPEED;
    }

    if (reserved_bullets[2] != nullptr) {
      reserved_bullets[2]->velocity =
          (player_position - position).rotated(90).normalized() * BULLETS_SPEED;
    }

    shooting_pattern = BossPattern::SUCCESSIVE_SHOTS;
    pattern_switching_cooldown =
        PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];

    break;
  }
  case BossPattern::SUCCESSIVE_SHOTS: {
    phase2_shooting_cooldown -= dt;

    if (phase2_shooting_cooldown <= 0) {
      for (auto &b : bullets) {
        if (!b.active) {
          b.active = true;
          b.position = {position.x, position.y + HEIGHT / 2};
          b.velocity = {0, BULLETS_SPEED};
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
        break;
      }
    }
    break;
  }
  }
}
