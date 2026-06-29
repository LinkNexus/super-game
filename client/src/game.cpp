#include "game.h"
#include "constants.h"
#include "raylib.h"

void Game::run() {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SUPER GAME");
  SetTargetFPS(TARGET_FPS);

  player.position = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT - 60.0f};
  float accumulator = 0.0f;

  while (!WindowShouldClose()) {
    float frame_time = GetFrameTime();
    if (frame_time > 0.1f) frame_time = 0.1f;
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

  for (auto& b : bullets)
    b.update(dt);
}

void Game::draw() {
  player.draw();

  for (const auto& b : bullets)
    b.draw();

  for (const auto& e : enemies)
    e.draw();

  boss.draw();

  DrawFPS(10, 10);
}
