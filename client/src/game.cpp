#include "game.h"
#include "constants.h"
#include "objects/enemy.h"
#include "raylib.h"
#include "rnd_generator.h"
#include <cfloat>

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
  player.update(dt);

  updateEnemies(dt);

  if (fire_cooldown_ > 0.0f)
    fire_cooldown_ -= dt;

  if (IsKeyDown(KEY_SPACE) && fire_cooldown_ <= 0.0f) {
    spawnBullet();
    fire_cooldown_ = FIRE_COOLDOWN;
  }

  for (auto &b : bullets)
    b.update(dt);
}

void Game::spawnBullet() {
  for (auto &b : bullets) {
    if (!b.active) {
      b.position = {player.position.x, player.position.y - player.size};
      b.velocity = {0.0f, -BULLET_SPEED};
      b.active = true;
      return;
    }
  }
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
      (ENEMY_COLS * Enemy::WIDTH) + (ENEMIES_SPACING_X * (ENEMY_COLS - 1));
  int offset_x = (SCREEN_WIDTH - enemy_pool_size) / 2;
  int current_idx = 0;

  for (auto &enemy : enemies) {
    enemy.alive = true;
    enemy.position.x = offset_x + (current_idx % ENEMY_COLS) *
                                      (Enemy::WIDTH + ENEMIES_SPACING_X);
    enemy.position.y = 50.0f + (current_idx / ENEMY_COLS) *
                                   (Enemy::HEIGHT + ENEMIES_SPACING_Y);

    enemy.type = (current_idx % 2 == 0) ? ENEMY_TYPE_1 : ENEMY_TYPE_2;

    current_idx++;
  }
}

void Game::updateEnemies(float dt) {
  int rnd_idx = RndGenerator::getRandomInt(0, ENEMY_ROWS * ENEMY_COLS - 1);

  float farthest_left = FLT_MAX;
  float farthest_right = -FLT_MAX;

  for (const auto &e : enemies) {
    if (!e.alive)
      continue;

    if (e.position.x < farthest_left) {
      farthest_left = e.position.x;
    }

    if (e.position.x > farthest_right) {
      farthest_right = e.position.x;
    }
  }

  if (farthest_left - Enemy::WIDTH / 2 <= 0.0f ||
      farthest_right >= SCREEN_WIDTH - Enemy::WIDTH / 2) {
    enemies_direction_ *= -1;
    float overflow;

    if (farthest_left - Enemy::WIDTH / 2 <= 0.0f) {
      overflow = -(farthest_left - Enemy::WIDTH / 2);
    } else {
      overflow = farthest_right - (SCREEN_WIDTH - Enemy::WIDTH / 2);
    }

    for (auto &e : enemies) {
      if (!e.alive)
        continue;

      e.position.y += ENEMIES_DESCENT_SPEED;
      e.position.x += enemies_direction_ * (overflow + 1);
    }

    return;
  }

  for (auto &e : enemies) {
    if (!e.alive)
      continue;

    e.position.x += enemies_direction_ * ENEMIES_CYCLE_SPEED * dt;
  }
}
