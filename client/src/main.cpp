#include "raylib.h"

static constexpr int SCREEN_WIDTH = 1200;
static constexpr int SCREEN_HEIGHT = 800;
static constexpr int TARGET_FPS = 144;
static constexpr float FIXED_DT = 1.0f / 60.0f;

struct Player {
  Vector2 position;
  float rotation = 0.0f;
  float speed = 300.0f;
  float size = 20.f;

  void update(float dt) {
    float vec_x = 0;

    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
      vec_x = -1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
      vec_x = 1.0f;

    position.x += vec_x * speed * dt;

    if (position.x < size)
      position.x = size;
    if (position.x > SCREEN_WIDTH - size)
      position.x = SCREEN_WIDTH - size;
    if (position.y < size)
      position.y = size;
    if (position.y > SCREEN_HEIGHT - size)
      position.y = SCREEN_WIDTH - size;
  }

  void draw() {
    Vector2 tip = {position.x, position.y - size};
    Vector2 left = {position.x - size * 0.7f, position.y + size * 0.7f};
    Vector2 right = {position.x + size * 0.7f, position.y + size * 0.7f};

    DrawTriangle(tip, left, right, SKYBLUE);
    DrawTriangleLines(tip, left, right, WHITE);
  }
};

int main(int argc, char *argv[]) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SUPER GAME");

  Player player;
  player.position = {SCREEN_HEIGHT / 2.0f, SCREEN_WIDTH / 2.0f};
  float accumulator = 0.0f;

  while (!WindowShouldClose()) {
    float frame_time = GetFrameTime();
    if (frame_time > 0.1f)
      frame_time = 0.1f;
    accumulator += frame_time;

    while (accumulator >= FIXED_DT) {
      player.update(FIXED_DT);
      accumulator -= FIXED_DT;
    }

    BeginDrawing();
    ClearBackground(BLACK);
    player.draw();
    DrawFPS(10, 10);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
