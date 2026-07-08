#pragma once
#include "raylib.h"

struct Star {
  Vector2 position;
  float   speed;
  float   size;
  Color   color;

  void init_random(int screen_width, int screen_height);
  void update(float dt, int screen_height, int screen_width);
  void draw() const;
};
