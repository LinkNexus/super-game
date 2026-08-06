#include "entities/boss.h"
#include "shared/messages.h"
#include "shared/sim/boss_sim.h"
#include <cstddef>

void Boss::loadTexture() { texture_ = LoadTexture("assets/boss.png"); }

void Boss::unload() { UnloadTexture(texture_); }

void Boss::draw(const shared::BossState &state, bool draw_health_bar) const {
  if (!state.active)
    return;

  float bar_x = (shared::SCREEN_WIDTH - HEALTH_BAR_WIDTH) / 2.0f;
  float health_ratio = (float)state.health / shared::BossSimState::INITIAL_LP;
  if (health_ratio < 0.0f)
    health_ratio = 0.0f;

  if (draw_health_bar) {
    DrawRectangle((int)bar_x, (int)HEALTH_BAR_Y, (int)HEALTH_BAR_WIDTH,
                  (int)HEALTH_BAR_HEIGHT, GRAY);
    DrawRectangle((int)bar_x, (int)HEALTH_BAR_Y,
                  (int)(HEALTH_BAR_WIDTH * health_ratio),
                  (int)HEALTH_BAR_HEIGHT, RED);
    DrawRectangleLines((int)bar_x, (int)HEALTH_BAR_Y, (int)HEALTH_BAR_WIDTH,
                       (int)HEALTH_BAR_HEIGHT, WHITE);
  }

  if (texture_.id != 0) {
    float scale = 80.0f / texture_.width;
    Vector2 draw_pos = {state.position.x - (texture_.width * scale) / 2.0f,
                        state.position.y - (texture_.height * scale) / 2.0f};
    DrawTextureEx(texture_, draw_pos, 0.0f, scale, WHITE);
  } else {
    DrawRectangle((int)state.position.x - 40, (int)state.position.y - 20, 80,
                  40, RED);
  }
}
