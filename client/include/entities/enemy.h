#pragma once

#include "raylib.h"
#include "shared/sim/enemy_sim.h"

struct Enemy {
  static void draw(float pos_x, float pos_y, shared::EnemyType type);

  static void loadTextures();
  static void unloadTextures();

private:
  static Texture2D texture_type1_;
  static Texture2D texture_type2_;
};
