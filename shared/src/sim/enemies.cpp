#include "shared/sim/entities/enemies.h"
#include "shared/rnd_generator.h"
#include "shared/sim/constants.h"
#include "shared/sim/entities/bullet.h"
#include "shared/sim/types.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <mathlib.hpp>

void shared::sim::EnemiesPoolSimState::placeEnemy(
    shared::sim::EnemySimState &enemy, std::size_t idx
) {
  enemy.position = offset + slot_fn(idx / shared::sim::ENEMY_COLS, idx % shared::sim::ENEMY_COLS);
}

void shared::sim::EnemySimState::spawnBullet(
    std::array<shared::sim::BulletSimState, MAX_BULLETS> &bullets
) const {
  for (auto &bullet : bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.velocity = {0, 0, -BulletSimState::SPEED};
      bullet.position = position + -mathlib::Vec3f::unitZ().scaleBy(SIZE / 2);
      bullet.type = shared::sim::BulletType::ENEMY;
      break;
    }
  }
}

void shared::sim::EnemiesPoolSimState::init(
    GetEntranceFn entranceFn, PlayerCount playersCount, EnemiesPoolSimState::GetSlotFn getSlotFn
) {
  if (auto res = std::find_if(
          ROWS_PER_PLAYERS_COUNT.begin(),
          ROWS_PER_PLAYERS_COUNT.end(),
          [playersCount](const auto &pair) { return pair.first == playersCount; }
      );
      res != ROWS_PER_PLAYERS_COUNT.end()) {
    active_rows = res->second;
  } else {
    active_rows = MAX_ENEMY_ROWS;
  }

  slot_fn = getSlotFn;
  entrance = entranceFn(active_rows);
  offset = entrance.trajectory.start;

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    auto &enemy = enemies[idx];

    if (idx / ENEMY_COLS >= active_rows) {
      enemy.alive = false;
      continue;
    }

    enemy.alive = true;
    placeEnemy(enemy, idx);

    enemy.type = (idx % 2 == 0) ? EnemyType::TYPE_1 : EnemyType::TYPE_2;
    enemy.shooting_cooldown = RndGenerator::getRandom(
        EnemySimState::SHOOTING_INTERVAL_MIN, EnemySimState::SHOOTING_INTERVAL_MAX
    );
  }
}

void shared::sim::EnemiesPoolSimState::stepEntrance(float dt) {
  offset.moveTowards(entrance.trajectory.end, entrance.speed * dt);

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    placeEnemy(enemies[idx], idx);
  }
}

bool shared::sim::EnemiesPoolSimState::allEnemiesDefeated() {
  return std::ranges::none_of(enemies, &EnemySimState::alive);
}

void shared::sim::EnemiesPoolSimState::step(
    mathlib::Vec3f offsetDelta, float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool
) {
  offset += offsetDelta;

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    auto &enemy = enemies[idx];

    if (!enemy.alive)
      continue;

    enemy.shooting_cooldown -= dt;

    if (enemy.shooting_cooldown <= 0.0f &&
        (idx + ENEMY_COLS >= enemies.size() || !enemies[idx + ENEMY_COLS].alive)) {
      bool ally_ahead = false;
      auto current_idx = idx + ENEMY_COLS;

      while (!ally_ahead && current_idx < enemies.size()) {
        ally_ahead = enemies[current_idx].alive;
        current_idx += ENEMY_COLS;
      }

      if (!ally_ahead) {
        enemy.spawnBullet(bullets_pool);
        enemy.shooting_cooldown = RndGenerator::getRandom(
            EnemySimState::SHOOTING_INTERVAL_MIN, EnemySimState::SHOOTING_INTERVAL_MAX
        );
      }
    }

    placeEnemy(enemy, idx);
  }
}

mathlib::Cuboidf shared::sim::EnemySimState::getHitBox() const {
  return mathlib::Cuboidf::fromCenterAndSize(position, SIZE);
}

bool shared::sim::EnemiesPoolSimState::isEntranceComplete() {
  return offset.approxEquals(entrance.trajectory.end);
}

std::optional<mathlib::Cuboidf> shared::sim::EnemiesPoolSimState::aliveExtents() const {
  std::optional<mathlib::Cuboidf> box{};

  for (const auto &enemy : enemies) {
    if (!enemy.alive)
      continue;

    box = box ? mathlib::Cuboidf::fromCorners(
                    box->min.minWith(enemy.position), box->max.maxWith(enemy.position)
                )
              : mathlib::Cuboidf::fromCorners(enemy.position, enemy.position);
  }

  return box;
}
