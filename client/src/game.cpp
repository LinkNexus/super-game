#include "game.h"
#include "constants.h"
#include "objects/enemy.h"
#include "raylib.h"
#include "rnd_generator.h"
#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <ranges>

void Game::run() {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SUPER GAME");
  SetTargetFPS(TARGET_FPS);

  player.position = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 60.0f};
  initEnemies();
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

  CloseWindow();
}

void Game::update(float dt) {
  player.update(dt, bullets);

  updateEnemies(dt);

  for (auto &b : bullets)
    b.update(dt);
}

void Game::draw() {
  player.draw();

  for (const auto &b : bullets)
    b.draw();

  for (const auto &e : enemies)
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
  auto alive_enemies_x =
      enemies | std::views::filter([](const Enemy &e) { return e.alive; }) |
      std::views::transform([](const Enemy &e) { return e.position.x; });

  auto farthest_left = *std::ranges::min_element(alive_enemies_x);
  auto farthest_right = *std::ranges::max_element(alive_enemies_x);

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
