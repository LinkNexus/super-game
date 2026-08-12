#pragma once

#include "raylib.h"
#include "shared/sim/enemy_sim.h"

/// Render-only enemy entity. Stateless: the grid's positions/types live in
/// the wire `GameState`, so drawing is just a static function taking a
/// position and type rather than an instance per enemy.
struct Enemy {
  static void draw(float pos_x, float pos_y, shared::EnemyType type);

  static void loadTextures();
  static void unloadTextures();

private:
  static Texture2D texture_type1_;
  static Texture2D texture_type2_;
};
