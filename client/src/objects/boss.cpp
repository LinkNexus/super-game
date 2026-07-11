#include "objects/boss.h"

void Boss::load_texture() {
  texture_ = LoadTexture("assets/boss.png");
}

void Boss::unload() {
  UnloadTexture(texture_);
}

void Boss::draw() const {
  if (!active) return;

  if (texture_.id != 0) {
    float scale = 80.0f / texture_.width;
    Vector2 draw_pos = {
      position.x - (texture_.width  * scale) / 2.0f,
      position.y - (texture_.height * scale) / 2.0f
    };
    DrawTextureEx(texture_, draw_pos, 0.0f, scale, WHITE);
  } else {
    DrawRectangle((int)position.x - 40, (int)position.y - 20, 80, 40, RED);
  }
}
