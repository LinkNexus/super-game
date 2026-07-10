#include "game.h"
#include "constants.h"
#include "objects/bullet.h"
#include "objects/enemy.h"
#include "objects/player.h"
#include "objects/star.h"
#include "raylib.h"
#include "rnd_generator.h"
#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdlib>
#include <format>
#include <iostream>
#include <string>

bool aabb(Vector2 pos_a, float hw_a, float hh_a, Vector2 pos_b, float hw_b,
          float hh_b) {
  return std::abs(pos_a.x - pos_b.x) <= (hw_a + hw_b) &&
         std::abs(pos_a.y - pos_b.y) <= (hh_a + hh_b);
}

void Game::run() {
  RndGenerator::seed();
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SUPER GAME");
  SetTargetFPS(TARGET_FPS);

  player.position = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 60.0f};
  player.load_texture();
  Enemy::loadTextures();
  boss.load_texture();
  initEnemies();

  for (auto &s : stars)
    s.init_random(SCREEN_WIDTH, SCREEN_HEIGHT);

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

  player.unload();
  Enemy::unloadTextures();
  boss.unload();
  CloseWindow();
}

void Game::update(float dt) {
  player.update(dt, bullets);

  updateEnemies(dt);

  for (auto &b : bullets)
    b.update(dt);

  for (auto &s : stars)
    s.update(dt, SCREEN_HEIGHT, SCREEN_WIDTH);

  checkCollisions();
}

void Game::draw() {
  for (const auto &s : stars)
    s.draw();

  player.draw();

  for (const auto &b : bullets)
    b.draw();

  for (const auto &e : enemies)
    if (e.alive)
      e.draw();

  boss.draw();

  DrawFPS(10, 10);
}

void Game::initEnemies() {
  int enemy_pool_size =
      (ENEMIES_COLS * Enemy::WIDTH) + (ENEMIES_SPACING_X * (ENEMIES_COLS - 1));
  int offset_x = (SCREEN_WIDTH - enemy_pool_size) / 2;

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    Enemy &enemy = enemies[idx];

    enemy.alive = true;
    enemy.position.x =
        offset_x + (idx % ENEMIES_COLS) * (Enemy::WIDTH + ENEMIES_SPACING_X);
    enemy.position.y =
        50.0f + (idx / ENEMIES_COLS) * (Enemy::HEIGHT + ENEMIES_SPACING_Y);

    enemy.type = (idx % 2 == 0) ? EnemyType::TYPE_1 : EnemyType::TYPE_2;
    enemy.shooting_cooldown = RndGenerator::getRandomFloat(
        ENEMIES_SHOOTING_INTERVAL_MIN, ENEMIES_SHOOTING_INTERVAL_MAX);
  }
}

void Game::updateEnemies(float dt) {
  float farthest_left  = FLT_MAX;
  float farthest_right = -FLT_MAX;
  for (const auto &e : enemies) {
    if (!e.alive) continue;
    if (e.position.x < farthest_left)  farthest_left  = e.position.x;
    if (e.position.x > farthest_right) farthest_right = e.position.x;
  }

  std::optional<float> overflow = std::nullopt;

  if (farthest_left - Enemy::WIDTH / 2 <= 0.0f) {
    enemies_direction_ *= -1;
    overflow = -(farthest_left - Enemy::WIDTH / 2);
  } else if (farthest_right >= SCREEN_WIDTH - Enemy::WIDTH / 2) {
    enemies_direction_ *= -1;
    overflow = farthest_right - (SCREEN_WIDTH - Enemy::WIDTH / 2);
  }

  for (std::size_t idx = 0; idx < enemies.size(); ++idx) {
    Enemy &enemy = enemies[idx];
    if (!enemy.alive)
      continue;

    enemy.shooting_cooldown -= dt;

    if (enemy.shooting_cooldown <= 0.0f &&
        (idx + ENEMIES_COLS >= enemies.size() ||
         !enemies[idx + ENEMIES_COLS].alive)) {

      bool ally_ahead = false;
      auto current_idx = idx + ENEMIES_COLS;

      while (!ally_ahead && current_idx < enemies.size()) {
        ally_ahead = enemies[current_idx].alive;
        current_idx += ENEMIES_COLS;
      }

      if (!ally_ahead) {
        enemy.spawnBullet(bullets);

        enemy.shooting_cooldown = RndGenerator::getRandomFloat(
            ENEMIES_SHOOTING_INTERVAL_MIN, ENEMIES_SHOOTING_INTERVAL_MAX);
      }
    }

    enemy.position.x += overflow
                            ? enemies_direction_ * (overflow.value() + 1)
                            : enemies_direction_ * ENEMIES_CYCLE_SPEED * dt;

    if (overflow) {
      enemy.position.y += ENEMIES_DESCENT_SPEED;
    }
  }
}

void Game::checkCollisions() {
  for (auto &bullet : bullets) {
    if (bullet.active) {
      if (bullet.type == BulletType::PLAYER) {
        for (auto &enemy : enemies) {
          if (enemy.alive &&
              aabb(bullet.position, Bullet::WIDTH / 2, Bullet::HEIGHT / 2,
                   enemy.position, Enemy::WIDTH / 2, Enemy::HEIGHT / 2)) {
            enemy.alive = false;
            bullet.active = false;
            break;
          }
        }
      } else if (bullet.type == BulletType::ENEMY &&
                 aabb(bullet.position, Bullet::WIDTH / 2, Bullet::HEIGHT / 2,
                      player.position, Player::SIZE * Player::HITBOX_SCALE / 2,
                      Player::SIZE * Player::HITBOX_SCALE / 2)) {
        std::cout << "Player loses a live, an enemy bullet touched him"
                  << std::endl;
        bullet.active = false;
      }
    }
  }

  for (const auto &enemy : enemies) {
    if (enemy.alive &&
        aabb(enemy.position, Enemy::WIDTH / 2, Enemy::HEIGHT / 2,
             player.position, Player::SIZE * Player::HITBOX_SCALE / 2,
             Player::SIZE * Player::HITBOX_SCALE / 2)) {
      std::cout << "Player is dead, an enemy touched him" << std::endl;
      break;
    }
  }
}
