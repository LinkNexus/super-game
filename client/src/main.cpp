#include "raylib.h"
#include "entities/Star.h"
#include "entities/Player.h"

static constexpr int SCREEN_WIDTH = 1200;
static constexpr int SCREEN_HEIGHT = 800;
static constexpr int TARGET_FPS = 144;
static constexpr float FIXED_DT = 1.0f / 60.0f;
static constexpr int STAR_COUNT = 150;  

int main(int argc, char *argv[]) {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SUPER GAME");

  Player player;
  player.position = {SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT/ 2.0f};
  player.load_texture();

  float accumulator = 0.0f;

  Star stars[STAR_COUNT];
  InitStarfield(stars, STAR_COUNT, SCREEN_WIDTH, SCREEN_HEIGHT);

  while (!WindowShouldClose()) {
    float frame_time = GetFrameTime();
    if (frame_time > 0.1f)
      frame_time = 0.1f;
    accumulator += frame_time;

    while (accumulator >= FIXED_DT) {
      player.update(FIXED_DT);
      UpdateStarfield(stars, STAR_COUNT, FIXED_DT, SCREEN_HEIGHT, SCREEN_WIDTH);
      accumulator -= FIXED_DT;
    }

    BeginDrawing();
    ClearBackground(BLACK);

    DrawStarfield(stars, STAR_COUNT);
    player.draw();
    DrawFPS(10, 10);
    EndDrawing();
  }
  player.unload();

  CloseWindow();
  return 0;
}
