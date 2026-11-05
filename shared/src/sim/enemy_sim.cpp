#include "shared/sim/enemy_sim.h"
#include "shared/constants.h"
#include "shared/rnd_generator.h"
#include "shared/sim/bullet_sim.h"
#include "shared/sim/player_sim.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <mathlib.hpp>
#include <optional>

using namespace shared;

void EnemySimState::spawnBullet(
    std::array<shared::BulletSimState, MAX_BULLETS> &bullets) const {
  for (auto &bullet : bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.velocity = {0, -BulletSimState::SPEED};
      bullet.position = position + mathlib::Vec2f{0, -HEIGHT / 2};
      bullet.type = shared::BulletType::ENEMY;
      break;
    }
  }
}

void EnemiesPoolSimState::stepEnemy(EnemySimState &enemy, std::size_t idx) {
  enemy.position.y =
      offset.y - (idx / ENEMY_COLS) * (EnemySimState::HEIGHT + SPACING_Y);
  enemy.position.x =
      offset.x + (idx % ENEMY_COLS) * (EnemySimState::WIDTH + SPACING_X);
}

void EnemiesPoolSimState::init(uint8_t playersCount) {
  if (auto res = std::find_if(ROWS_PER_PLAYERS_COUNT.begin(),
                              ROWS_PER_PLAYERS_COUNT.end(),
                              [playersCount](const auto &pair) {
                                return pair.first == playersCount;
                              });
      res != ROWS_PER_PLAYERS_COUNT.end()) {
    active_rows = res->second;
  } else {
    active_rows = MAX_ENEMY_ROWS;
  }

  const float pool_size_x =
      (ENEMY_COLS * EnemySimState::WIDTH) + (SPACING_X * (ENEMY_COLS - 1));
  offset.set((SCREEN_WIDTH - pool_size_x) / 2.0f,
             MIN_INITIAL_OFFSET_Y + (active_rows - 1) * ROW_PITCH);
  direction = 1;
  alive_enemies_count = 0;

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    auto &enemy = enemies[idx];

    if (idx / ENEMY_COLS >= active_rows) {
      enemy.alive = false;
      continue;
    }

    enemy.alive = true;
    stepEnemy(enemy, idx);

    enemy.type = (idx % 2 == 0) ? EnemyType::TYPE_1 : EnemyType::TYPE_2;
    enemy.shooting_cooldown =
        RndGenerator::getRandomFloat(EnemySimState::SHOOTING_INTERVAL_MIN,
                                     EnemySimState::SHOOTING_INTERVAL_MAX);
  }
}

void EnemiesPoolSimState::stepEntrance(float dt) {
  if (offset.y > FINAL_OFFSET_Y) {
    offset.y = std::max(offset.y - INITIAL_DESCENT_SPEED * dt, FINAL_OFFSET_Y);

    for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
      stepEnemy(enemies[idx], idx);
    }
  }
}

bool EnemiesPoolSimState::isEntranceComplete() {
  return offset.y <= FINAL_OFFSET_Y;
}

bool EnemiesPoolSimState::allEnemiesDefeated() {
  return alive_enemies_count == 0;
}

void EnemiesPoolSimState::step(
    float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool) {
  auto farthestLeft = FLT_MAX;
  auto farthestRight = -FLT_MAX;

  alive_enemies_count = 0;

  for (const auto &enemy : enemies) {
    if (!enemy.alive)
      continue;

    alive_enemies_count++;

    if (enemy.position.x < farthestLeft)
      farthestLeft = enemy.position.x;

    if (enemy.position.x > farthestRight)
      farthestRight = enemy.position.x;
  }

  std::optional<float> overflow = std::nullopt;

  if (farthestLeft - EnemySimState::WIDTH / 2 <= 0.0f) {
    direction *= -1;
    overflow = -(farthestLeft - EnemySimState::WIDTH / 2);
  } else if (farthestRight >= SCREEN_WIDTH - EnemySimState::WIDTH / 2) {
    direction *= -1;
    overflow = farthestRight - (SCREEN_WIDTH - EnemySimState::WIDTH / 2);
  }

  offset +=
      {direction * (overflow ? (overflow.value() + 1) : (CYCLE_SPEED * dt)),
       -(overflow ? DESCENT_SPEED : 0)};

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    auto &enemy = enemies[idx];

    if (!enemy.alive)
      continue;

    enemy.shooting_cooldown -= dt;

    if (enemy.shooting_cooldown <= 0.0f &&
        (idx + ENEMY_COLS >= enemies.size() ||
         !enemies[idx + ENEMY_COLS].alive)) {
      bool ally_ahead = false;
      auto current_idx = idx + ENEMY_COLS;

      while (!ally_ahead && current_idx < enemies.size()) {
        ally_ahead = enemies[current_idx].alive;
        current_idx += ENEMY_COLS;
      }

      if (!ally_ahead) {
        enemy.spawnBullet(bullets_pool);
        enemy.shooting_cooldown =
            RndGenerator::getRandomFloat(EnemySimState::SHOOTING_INTERVAL_MIN,
                                         EnemySimState::SHOOTING_INTERVAL_MAX);
      }
    }

    stepEnemy(enemy, idx);
  }
}

bool EnemiesPoolSimState::reachedPlayer() {
  for (const auto &enemy : enemies) {
    if (enemy.alive && enemy.position.y - EnemySimState::HEIGHT / 2 <=
                           PlayerSimState::POSITION_Y + PlayerSimState::SIZE) {
      return true;
    }
  }

  return false;
}

mathlib::Rectanglef shared::EnemySimState::getHitBox() const {
  return mathlib::Rectanglef::fromCenterAndSize(position, WIDTH, HEIGHT);
}
