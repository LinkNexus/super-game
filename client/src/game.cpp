#include "game.h"
#include "constants.h"
#include "entities/bullet.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/star.h"
#include "raylib.h"
#include "rnd_generator.h"
#include <cfloat>
#include <cstdlib>
#include <iostream>

bool aabb(Vector2 pos_a, float hw_a, float hh_a, Vector2 pos_b, float hw_b,
          float hh_b) {
  return std::abs(pos_a.x - pos_b.x) <= (hw_a + hw_b) &&
         std::abs(pos_a.y - pos_b.y) <= (hh_a + hh_b);
}

void Game::run() {
  RndGenerator::seed();
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SUPER GAME");
  SetTargetFPS(TARGET_FPS);

  player_.position = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 60.0f};
  player_.loadTexture();
  Enemy::loadTextures();
  boss_.loadTexture();
  initEnemies();

  for (auto &s : stars_)
    s.initRandom(SCREEN_WIDTH, SCREEN_HEIGHT);

  float accumulator = 0.0f;

  while (!WindowShouldClose()) {
    float frame_time = GetFrameTime();
    if (frame_time > 0.1f)
      frame_time = 0.1f;
    accumulator += frame_time;

    while (accumulator >= FIXED_DT) {
      update(FIXED_DT);
      accumulator -= FIXED_DT;
    }

    BeginDrawing();
    ClearBackground(BLACK);
    draw();
    EndDrawing();
  }

  player_.unload();
  Enemy::unloadTextures();
  boss_.unload();
  CloseWindow();
}

void Game::update(float dt) {
  player_.update(dt, bullets_, !boss_entrance_running_);

  if (boss_phase_) {
    if (boss_entrance_running_)
      animateBossEntrance(dt);
    else if (boss_.active)
      updateBoss(dt);
  } else
    updateEnemies(dt);

  for (auto &b : bullets_)
    b.update(dt);

  for (auto &s : stars_)
    s.update(dt, SCREEN_HEIGHT, SCREEN_WIDTH);

  checkCollisions();
}

void Game::draw() {
  for (const auto &s : stars_)
    s.draw();

  player_.draw();

  for (const auto &b : bullets_)
    b.draw();

  for (const auto &e : enemies_)
    if (e.alive)
      e.draw();

  boss_.draw();

  DrawFPS(10, 10);
}

void Game::initEnemies() {
  int enemy_pool_size =
      (ENEMIES_COLS * Enemy::WIDTH) + (ENEMIES_SPACING_X * (ENEMIES_COLS - 1));
  int offset_x = (SCREEN_WIDTH - enemy_pool_size) / 2;

  for (std::size_t idx = 0; idx < enemies_.size(); ++idx) {
    Enemy &enemy = enemies_[idx];

    enemy.alive = true;
    enemy.position.x =
        offset_x + (idx % ENEMIES_COLS) * (Enemy::WIDTH + ENEMIES_SPACING_X);
    enemy.position.y =
        50.0f + (idx / ENEMIES_COLS) * (Enemy::HEIGHT + ENEMIES_SPACING_Y);

    enemy.type = (idx % 2 == 0) ? EnemyType::TYPE_1 : EnemyType::TYPE_2;
    enemy.shooting_cooldown = RndGenerator::getRandomFloat(
        Enemy::SHOOTING_INTERVAL_MIN, Enemy::SHOOTING_INTERVAL_MAX);
  }
}

void Game::updateEnemies(float dt) {
  auto farthest_left = FLT_MAX;
  auto farthest_right = -FLT_MAX;

  auto alive_enemies_count = 0;

  for (const auto &enemy : enemies_) {
    if (!enemy.alive)
      continue;

    alive_enemies_count++;

    if (enemy.position.x < farthest_left)
      farthest_left = enemy.position.x;

    if (enemy.position.x > farthest_right)
      farthest_right = enemy.position.x;
  }

  if (alive_enemies_count == 0) {
    boss_phase_ = true;
    initBoss();
    return;
  }

  std::optional<float> overflow = std::nullopt;

  if (farthest_left - Enemy::WIDTH / 2 <= 0.0f) {
    enemies_direction_ *= -1;
    overflow = -(farthest_left - Enemy::WIDTH / 2);
  } else if (farthest_right >= SCREEN_WIDTH - Enemy::WIDTH / 2) {
    enemies_direction_ *= -1;
    overflow = farthest_right - (SCREEN_WIDTH - Enemy::WIDTH / 2);
  }

  for (std::size_t idx = 0; idx < enemies_.size(); ++idx) {
    Enemy &enemy = enemies_[idx];
    if (!enemy.alive)
      continue;

    enemy.shooting_cooldown -= dt;

    if (enemy.shooting_cooldown <= 0.0f &&
        (idx + ENEMIES_COLS >= enemies_.size() ||
         !enemies_[idx + ENEMIES_COLS].alive)) {

      bool ally_ahead = false;
      auto current_idx = idx + ENEMIES_COLS;

      while (!ally_ahead && current_idx < enemies_.size()) {
        ally_ahead = enemies_[current_idx].alive;
        current_idx += ENEMIES_COLS;
      }

      if (!ally_ahead) {
        enemy.spawnBullet(bullets_);

        enemy.shooting_cooldown = RndGenerator::getRandomFloat(
            Enemy::SHOOTING_INTERVAL_MIN, Enemy::SHOOTING_INTERVAL_MAX);
      }
    }

    enemy.position.x += overflow ? enemies_direction_ * (overflow.value() + 1)
                                 : enemies_direction_ * Enemy::CYCLE_SPEED * dt;

    if (overflow) {
      enemy.position.y += Enemy::DESCENT_SPEED;
    }
  }
}

void Game::checkCollisions() {
  for (auto &bullet : bullets_) {
    if (bullet.active) {
      if (bullet.type == BulletType::PLAYER) {

        if (boss_phase_) {
          if (boss_.active &&
              aabb(bullet.position, Bullet::WIDTH / 2, Bullet::HEIGHT / 2,
                   boss_.position, Boss::WIDTH / 2, Boss::HEIGHT / 2)) {
            boss_.health--;
            bullet.active = false;

            if (boss_.health <= 0) {
              boss_.active = false;
              std::cout << "Boss defeated!" << std::endl;
            }
          }
        } else {
          for (auto &enemy : enemies_) {
            if (enemy.alive &&
                aabb(bullet.position, Bullet::WIDTH / 2, Bullet::HEIGHT / 2,
                     enemy.position, Enemy::WIDTH / 2, Enemy::HEIGHT / 2)) {
              enemy.alive = false;
              bullet.active = false;
              break;
            }
          }
        }
      } else if (bullet.type == BulletType::ENEMY &&
                 aabb(bullet.position, Bullet::WIDTH / 2, Bullet::HEIGHT / 2,
                      player_.position, Player::SIZE * Player::HITBOX_SCALE / 2,
                      Player::SIZE * Player::HITBOX_SCALE / 2)) {
        std::cout << "Player loses a live, an enemy bullet touched him"
                  << "\n";
        bullet.active = false;
      }
    }
  }

  for (const auto &enemy : enemies_) {
    if (enemy.alive &&
        aabb(enemy.position, Enemy::WIDTH / 2, Enemy::HEIGHT / 2,
             player_.position, Player::SIZE * Player::HITBOX_SCALE / 2,
             Player::SIZE * Player::HITBOX_SCALE / 2)) {
      std::cout << "Player is dead, an enemy touched him" << std::endl;
      break;
    }
  }
}

void Game::initBoss() {
  boss_.active = true;
  boss_.position.y = -10.0f;
  boss_.position.x = (SCREEN_WIDTH - Boss::WIDTH) / 2;
  boss_entrance_running_ = true;
}

void Game::animateBossEntrance(float dt) {
  if (boss_.position.y < BOSS_POSITION_Y) {
    boss_.position.y = std::min(
        boss_.position.y + BOSS_INITIAL_DESCENT_SPEED * dt, BOSS_POSITION_Y);
    return;
  }
  boss_entrance_running_ = false;
}

void Game::updateBoss(float dt) {
  std::optional<float> overflow = std::nullopt;

  if (boss_.position.x - Boss::WIDTH / 2 <= 0.0f) {
    enemies_direction_ *= -1;
    overflow = -(boss_.position.x - Boss::WIDTH / 2);
  } else if (boss_.position.x >= SCREEN_WIDTH - Boss::WIDTH / 2) {
    enemies_direction_ *= -1;
    overflow = boss_.position.x - (SCREEN_WIDTH - Boss::WIDTH / 2);
  }

  boss_.position.x += overflow ? enemies_direction_ * (overflow.value() + 1)
                               : enemies_direction_ * BOSS_CYCLE_SPEED * dt;

  if (boss_.current_phase < Boss::PHASES_COUNT &&
      boss_.health <= Boss::MIN_LP_PER_SWITCH[boss_.current_phase - 1]) {
    boss_.current_phase++;
    boss_.pattern_switching_cooldown =
        Boss::PATTERN_SWITCHING_COOLDOWNS_PER_PHASE[boss_.current_phase - 1];
  }

  if (boss_.pattern_switching_cooldown <= 0) {
    boss_.spawnBullets(dt, bullets_, player_.position);
  } else {
    boss_.pattern_switching_cooldown -= dt;
  }
}
