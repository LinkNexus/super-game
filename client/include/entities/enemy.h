#pragma once

#include "raylib.h"
#include "shared/enemy_sim.h"

struct Enemy {
  void draw(float pos_x, float pos_y, bool alive, shared::EnemyType type) const;

  static void loadTextures();
  static void unloadTextures();

private:
  static Texture2D texture_type1_;
  static Texture2D texture_type2_;
};
