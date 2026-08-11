#include "shared/sim/boss_sim.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/sim/bullet_sim.h"

using namespace shared;

void BossSimState::init(uint8_t playersCount) {
  if (auto res = std::find_if(
          SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT.begin(),
          SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT.end(),
          [playersCount](const auto &p) { return p.first == playersCount; });
      res != SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT.end()) {
    spread_shot_bullets_count = res->second;
  } else {
    spread_shot_bullets_count = MAX_SPREAD_SHOT_BULLETS;
  }

  if (auto res = std::find_if(
          SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT.begin(),
          SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT.end(),
          [playersCount](const auto &p) { return p.first == playersCount; });
      res != SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT.end()) {
    successive_shots_bullets_count = res->second;
  } else {
    successive_shots_bullets_count = MAX_SUCCESSIVE_SHOTS_BULLETS;
  }

  position = {(SCREEN_WIDTH - BossSimState::WIDTH) / 2, INITIAL_POSITION_Y};
  health = max_health = INITIAL_LP * playersCount;
  active = true;
  current_phase = 1;
  direction = 1;
  pattern_switching_cooldown =
      PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
  shooting_pattern = BossPattern::SPREAD_SHOT;

  phase2_shooting_cooldown = PHASE2_SHOOTING_COOLDOWN;
  phase2_bullets_shot = 0;
}

void BossSimState::stepEntrance(float dt) {
  if (position.y < FINAL_POSITION_Y) {
    position.y =
        std::min(position.y + INITIAL_DESCENT_SPEED * dt, FINAL_POSITION_Y);
  }
}

bool BossSimState::isEntranceComplete() {
  return position.y >= FINAL_POSITION_Y;
}

void BossSimState::spawnBullets(
    float dt, std::array<shared::BulletSimState, MAX_BULLETS> &bullets) {
  switch (shooting_pattern) {
  case BossPattern::SPREAD_SHOT: {
    std::vector<shared::BulletSimState *> reserved_bullets(
        spread_shot_bullets_count, nullptr);

    int count = 0;

    for (auto &b : bullets) {
      if (count == spread_shot_bullets_count)
        break;

      if (!b.active) {
        reserved_bullets[count] = &b;
        count++;
      }
    }

    Vec2D spawnPosition{position.x, position.y + HEIGHT / 2};

    Vec2D bottomLeft{0, SCREEN_HEIGHT};
    Vec2D bottomRight{SCREEN_WIDTH, SCREEN_HEIGHT};

    auto direction1 = (bottomLeft - spawnPosition);
    auto direction2 = (bottomRight - spawnPosition);
    auto angle = direction1.angle_between(direction2);

    Vec2D straightDown{0, 1};

    for (std::size_t i = 0; i < spread_shot_bullets_count; ++i) {
      auto bullet = reserved_bullets[i];
      if (bullet == nullptr)
        continue;

      float angleOffset = 0.0f;
      if (spread_shot_bullets_count > 1) {
        angleOffset = -angle / 2 + (angle / (reserved_bullets.size() - 1)) * i;
      }

      bullet->velocity = straightDown.rotated(angleOffset) * BULLETS_SPEED;
      bullet->active = true;
      bullet->position = spawnPosition;
      bullet->type = BulletType::ENEMY;
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

      if (phase2_bullets_shot == MAX_SUCCESSIVE_SHOTS_BULLETS) {
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

void BossSimState::step(float dt,
                        std::array<BulletSimState, MAX_BULLETS> &bullets) {
  std::optional<float> overflow = std::nullopt;

  if (position.x - WIDTH / 2 <= 0.0f) {
    direction *= -1;
    overflow = -(position.x - WIDTH / 2);
  } else if (position.x >= SCREEN_WIDTH - WIDTH / 2) {
    direction *= -1;
    overflow = position.x - (SCREEN_WIDTH - WIDTH / 2);
  }

  position += {
      direction * (overflow ? (overflow.value() + 1) : CYCLE_SPEED * dt),
  };

  if (current_phase < PHASES_COUNT &&
      health <= MIN_LP_PER_SWITCH[current_phase - 1] * max_health) {
    current_phase++;
    pattern_switching_cooldown =
        PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
  }

  if (pattern_switching_cooldown > 0) {
    pattern_switching_cooldown -= dt;
  } else
    spawnBullets(dt, bullets);
}
