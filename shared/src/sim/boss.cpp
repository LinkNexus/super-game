#include "shared/sim/entities/boss.h"
#include "shared/sim/constants.h"
#include "shared/sim/entities/bullet.h"
#include "shared/sim/types.h"
#include <mathlib.hpp>

void shared::sim::BossSimState::init(LinearEntrance entrance, PlayerCount playersCount) {
  if (auto res = std::find_if(
          SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT.begin(),
          SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT.end(),
          [playersCount](const auto &p) { return p.first == playersCount; }
      );
      res != SPREAD_SHOT_BULLETS_COUNT_PER_PLAYERS_COUNT.end()) {
    spread_shot_bullets_count = res->second;
  } else {
    spread_shot_bullets_count = MAX_SPREAD_SHOT_BULLETS;
  }

  if (auto res = std::find_if(
          SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT.begin(),
          SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT.end(),
          [playersCount](const auto &p) { return p.first == playersCount; }
      );
      res != SUCCESSIVE_SHOTS_BULLETS_COUNT_PER_PLAYERS_COUNT.end()) {
    successive_shots_bullets_count = res->second;
  } else {
    successive_shots_bullets_count = MAX_SUCCESSIVE_SHOTS_BULLETS;
  }

  this->entrance = entrance;
  position = this->entrance.trajectory.start;
  health = max_health = INITIAL_LP * playersCount;
  active = true;
  current_phase = 1;
  pattern_switching_cooldown = PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
  shooting_pattern = BossPattern::SPREAD_SHOT;

  phase2_shooting_cooldown = PHASE2_SHOOTING_COOLDOWN;
  phase2_bullets_shot = 0;
}

void shared::sim::BossSimState::spawnBullets(
    BossSimState::BulletsSpreadFn spreadFn,
    float dt,
    const std::array<std::optional<mathlib::Vec3f>, MAX_PLAYERS_COOP> &playersPositions,
    std::array<BulletSimState, MAX_BULLETS> &bullets
) {
  mathlib::Vec3f bulletSpawnPosition =
      position + -mathlib::Vec3f::unitZ().scaleBy(BossSimState::SIZE / 2);

  switch (shooting_pattern) {
  case BossPattern::SPREAD_SHOT: {
    std::array<mathlib::Vec3f, MAX_SPREAD_SHOT_BULLETS> bulletsDirections{};

    auto out = std::span(bulletsDirections).first(spread_shot_bullets_count);

    spreadFn(bulletSpawnPosition, out);

    std::size_t next = 0;
    for (const auto &dir : out) {
      while (next < bullets.size() && bullets[next].active)
        ++next;
      if (next == bullets.size())
        break;

      auto &bullet = bullets[next++];
      bullet.active = true;
      bullet.position = bulletSpawnPosition;
      bullet.velocity = dir * BULLETS_SPEED;
      bullet.type = BulletType::ENEMY;
    }

    shooting_pattern = BossPattern::SUCCESSIVE_SHOTS;
    pattern_switching_cooldown = PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];

    break;
  }
  case BossPattern::SUCCESSIVE_SHOTS: {
    phase2_shooting_cooldown -= dt;

    if (phase2_shooting_cooldown <= 0) {
      for (const auto &p : playersPositions) {
        if (!p)
          continue;
        for (auto &b : bullets) {
          if (b.active)
            continue;

          b.active = true;
          b.position = bulletSpawnPosition;
          b.velocity = (*p - position).normalized() * BULLETS_SPEED;
          break;
        }
      }

      phase2_bullets_shot++;
      phase2_shooting_cooldown = PHASE2_SHOOTING_COOLDOWN;

      if (phase2_bullets_shot == MAX_SUCCESSIVE_SHOTS_BULLETS) {
        shooting_pattern = BossPattern::SPREAD_SHOT;
        phase2_bullets_shot = 0;
        pattern_switching_cooldown = PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
        break;
      }
    }
    break;
  }
  }
}

void shared::sim::BossSimState::step(
    BossSimState::BulletsSpreadFn spreadFn,
    const mathlib::Vec3f &newPosition,
    const std::array<std::optional<mathlib::Vec3f>, MAX_PLAYERS_COOP> &playersPositions,
    float dt,
    std::array<BulletSimState, MAX_BULLETS> &bullets
) {
  position = newPosition;

  if (current_phase < PHASES_COUNT && health <= MIN_LP_PER_SWITCH[current_phase - 1] * max_health) {
    current_phase++;
    pattern_switching_cooldown = PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[current_phase - 1];
  }

  if (pattern_switching_cooldown > 0) {
    pattern_switching_cooldown -= dt;
  } else
    spawnBullets(spreadFn, dt, playersPositions, bullets);
}

void shared::sim::BossSimState::stepEntrance(float dt) {
  position.moveTowards(entrance.trajectory.end, entrance.speed * dt);
}

mathlib::Cuboidf shared::sim::BossSimState::getHitBox() const {
  return mathlib::Cuboidf::fromCenterAndSize(position, SIZE);
}

bool shared::sim::BossSimState::isEntranceComplete() const {
  return position.approxEquals(entrance.trajectory.end);
}
