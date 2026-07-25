#include "shared/sim/enemy_sim.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/rnd_generator.h"
#include "shared/sim/bullet_sim.h"
#include "shared/sim/player_sim.h"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <optional>

using namespace shared;

void EnemySimState::spawnBullet(
    std::array<shared::BulletSimState, MAX_BULLETS> &bullets) const {
  for (auto &bullet : bullets) {
    if (!bullet.active) {
      bullet.active = true;
      bullet.velocity = {0, BulletSimState::SPEED};
      bullet.position = {position.x, position.y + HEIGHT / 2};
      bullet.type = shared::BulletType::ENEMY;
      break;
    }
  }
}

void EnemiesPoolSimState::stepEnemy(EnemySimState &enemy, std::size_t idx) {
  enemy.position.y =
      offset_y + (idx / COLS) * (EnemySimState::HEIGHT + SPACING_Y);
  enemy.position.x =
      offset_x + (idx % COLS) * (EnemySimState::WIDTH + SPACING_X);
}

void EnemiesPoolSimState::init() {
  int pool_size_x = (COLS * EnemySimState::WIDTH) + (SPACING_X * (COLS - 1));
  offset_x = (SCREEN_WIDTH - pool_size_x) / 2.0f;
  offset_y = INITIAL_OFFSET_Y;
  direction = 1;

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    auto &enemy = enemies[idx];

    enemy.alive = true;
    stepEnemy(enemy, idx);

    enemy.type = (idx % 2 == 0) ? EnemyType::TYPE_1 : EnemyType::TYPE_2;
    enemy.shooting_cooldown =
        RndGenerator::getRandomFloat(EnemySimState::SHOOTING_INTERVAL_MIN,
                                     EnemySimState::SHOOTING_INTERVAL_MAX);
  }
}

void EnemiesPoolSimState::stepEntrance(float dt) {
  if (offset_y < FINAL_OFFSET_Y) {
    offset_y = std::min(offset_y + INITIAL_DESCENT_SPEED * dt, FINAL_OFFSET_Y);

    for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
      stepEnemy(enemies[idx], idx);
    }
  }
}

bool EnemiesPoolSimState::isEntranceComplete() {
  return offset_y >= FINAL_OFFSET_Y;
}

bool EnemiesPoolSimState::allEnemiesDefeated() {
  return alive_enemies_count == 0;
}

void EnemiesPoolSimState::step(
    float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool) {
  auto farthest_left = FLT_MAX;
  auto farthest_right = -FLT_MAX;

  alive_enemies_count = 0;

  for (const auto &enemy : enemies) {
    if (!enemy.alive)
      continue;

    alive_enemies_count++;

    if (enemy.position.x < farthest_left)
      farthest_left = enemy.position.x;

    if (enemy.position.x > farthest_right)
      farthest_right = enemy.position.x;
  }

  std::optional<float> overflow = std::nullopt;

  if (farthest_left - EnemySimState::WIDTH / 2 <= 0.0f) {
    direction *= -1;
    overflow = -(farthest_left - EnemySimState::WIDTH / 2);
  } else if (farthest_right >= SCREEN_WIDTH - EnemySimState::WIDTH / 2) {
    direction *= -1;
    overflow = farthest_right - (SCREEN_WIDTH - EnemySimState::WIDTH / 2);
  }

  offset_x =
      offset_x + direction * (overflow ? (overflow.value() + 1)
                                       : (EnemySimState::CYCLE_SPEED * dt));
  offset_y = offset_y + (overflow ? EnemySimState::DESCENT_SPEED : 0);

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    auto &enemy = enemies[idx];

    if (!enemy.alive)
      continue;

    enemy.shooting_cooldown -= dt;

    if (enemy.shooting_cooldown <= 0.0f &&
        (idx + COLS >= enemies.size() || !enemies[idx + COLS].alive)) {
      bool ally_ahead = false;
      auto current_idx = idx + COLS;

      while (!ally_ahead && current_idx < enemies.size()) {
        ally_ahead = enemies[current_idx].alive;
        current_idx += COLS;
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
    if (enemy.position.y + EnemySimState::HEIGHT / 2 >=
        PlayerSimState::POSITION_Y - PlayerSimState::SIZE) {
      return true;
    }
  }

  return false;
}
