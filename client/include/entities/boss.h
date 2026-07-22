#pragma once
#include "raylib.h"
#include "shared/messages.h"

struct Boss {
  void draw(const shared::BossState &state) const;
  void loadTexture();
  void unload();

  static constexpr float HEALTH_BAR_WIDTH = 300.0f;
  static constexpr float HEALTH_BAR_HEIGHT = 20.0f;
  static constexpr float HEALTH_BAR_Y = 10.0f;

private:
  Texture2D texture_ = {};
};
