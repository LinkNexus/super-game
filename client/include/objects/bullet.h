#pragma once
#include "raylib.h"

struct Bullet {
  Vector2 position = {0, 0};
  Vector2 velocity = {0, 0};
  bool    active   = false;

  void update(float dt);
  void draw() const;
};
