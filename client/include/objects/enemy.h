#pragma once
#include "raylib.h"

typedef enum { ENEMY_TYPE_1, ENEMY_TYPE_2 } EnemyType;

struct Enemy {
  Vector2 position = {0, 0};
  EnemyType type = ENEMY_TYPE_1;
  bool alive = false;

  void draw() const;

  static constexpr float WIDTH = 32.0f;
  static constexpr float HEIGHT = 20.0f;
};
