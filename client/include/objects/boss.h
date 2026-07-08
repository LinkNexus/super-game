#pragma once
#include "raylib.h"

struct Boss {
  Vector2 position = {0, 0};
  int     health   = 20;
  bool    active   = false;

  void draw() const;
};
