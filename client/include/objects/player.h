#pragma once
#include "raylib.h"

struct Player {
  Vector2 position = {0, 0};
  float   speed    = 300.0f;
  float   size     = 20.0f;

  void update(float dt);
  void draw() const;
};
