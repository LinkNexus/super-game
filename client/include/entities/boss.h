#pragma once
#include "raylib.h"
#include "shared/messages.h"

struct Boss {
  void draw(const shared::BossState &state) const;
  void loadTexture();
  void unload();

private:
  Texture2D texture_ = {};
};
