#include "shared/sim/boss_sim.h"
#include "shared/constants.h"
#include "shared/math_utils.h"

using namespace shared;

void BossSimState::init() {
  position = {(SCREEN_WIDTH - BossSimState::WIDTH) / 2, INITIAL_POSITION_Y};
  health = INITIAL_LP;
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
    float dt, std::array<shared::BulletSimState, MAX_BULLETS> &bullets,
    std::array<std::optional<Vec2D>, MAX_PLAYERS> &players_positions) {
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
      b->type = BulletType::ENEMY;
    }

    auto it = std::find_if(players_positions.begin(), players_positions.end(),
                           [](const auto &p) { return p.has_value(); });

    if (reserved_bullets[0] != nullptr) {
      reserved_bullets[0]->velocity = {0, BULLETS_SPEED};
    }

    if (reserved_bullets[1] != nullptr) {
      if (it == players_positions.end())
        reserved_bullets[1]->velocity = {0, BULLETS_SPEED};
      else
        reserved_bullets[1]->velocity =
            (it->value() - position).normalized() * BULLETS_SPEED;
    }

    if (reserved_bullets[2] != nullptr) {
      if (it == players_positions.end())
        reserved_bullets[2]->velocity = {0, BULLETS_SPEED};
      else {
        Vec2D vec = {position.x - it->value().x, it->value().y - position.y};
        reserved_bullets[2]->velocity = vec.normalized() * BULLETS_SPEED;
      }
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

void BossSimState::step(
    float dt, std::array<BulletSimState, MAX_BULLETS> &bullets,
    std::array<std::optional<Vec2D>, MAX_PLAYERS> &player_positions) {
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
      health <= MIN_LP_PER_SWITCH[current_phase - 1]) {
    current_phase++;
    pattern_switching_cooldown =
        PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
  }

  if (pattern_switching_cooldown > 0) {
    pattern_switching_cooldown -= dt;
  } else
    spawnBullets(dt, bullets, player_positions);
}
