#pragma once

#include "raylib.h"
#include "shared/player_sim.h"

struct Player {
  Texture2D texture = {};

  void loadTexture();
  void unload();
  void draw(const shared::PlayerSimState &state) const;
  static Texture2D heart_texture_;
};
