#pragma once
#include "raylib.h"

/// Purely decorative parallax-background star, client-only (no equivalent
/// in the shared simulation).
struct Star {
  Vector2 position;
  float speed;
  float size;
  Color color;

  /// Places this star at a random position/speed/size within the screen
  /// bounds.
  void initRandom(int screen_width, int screen_height);

  /// Moves the star downward and wraps it back to the top once it leaves
  /// the screen.
  void update(float dt, int screen_height, int screen_width);

  void draw() const;
};
