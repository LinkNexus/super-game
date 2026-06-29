#pragma once
#include "raylib.h"

struct Enemy {
  Vector2 position = {0, 0};
  int     type     = 0;
  bool    alive    = false;

  void draw() const;
};
