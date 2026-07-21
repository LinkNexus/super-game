#include "entities/boss.h"
#include "constants.h"
#include "shared/messages.h"
#include <cstddef>

void Boss::loadTexture() { texture_ = LoadTexture("assets/boss.png"); }

void Boss::unload() { UnloadTexture(texture_); }

void Boss::draw(const shared::BossState &state) const {
  if (!state.active)
    return;

  if (texture_.id != 0) {
    float scale = 80.0f / texture_.width;
    Vector2 draw_pos = {state.position.x - (texture_.width * scale) / 2.0f,
                        state.position.y - (texture_.height * scale) / 2.0f};
    DrawTextureEx(texture_, draw_pos, 0.0f, scale, WHITE);
  } else {
    DrawRectangle((int)state.position.x - 40, (int)state.position.y - 20, 80,
                  40, RED);
  }

  const char *text = TextFormat("Boss Health: %d", state.health);
  DrawText(text, SCREEN_WIDTH - 10 - MeasureText(text, STATUS_FONT_SIZE),
           10 + STATUS_FONT_SIZE * 2, STATUS_FONT_SIZE, WHITE);
}
