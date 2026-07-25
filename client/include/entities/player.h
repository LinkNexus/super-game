#pragma once

#include "raylib.h"
#include "shared/messages.h"

struct Player {
  Texture2D texture = {};

  void loadTexture();
  void unload();
  void draw(const shared::PlayerState &state) const;

  static Texture2D heart_texture_;
  static constexpr float HEART_SIZE = 32.0f;
  static constexpr float HEART_SPACING = 4.0f;
};
