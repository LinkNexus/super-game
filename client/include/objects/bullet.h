#pragma once
#include "raylib.h"

typedef enum { PLAYER_BULLET, ENEMY_BULLET } BulletType;

struct Bullet {
  Vector2 position = {0, 0};
  Vector2 velocity = {0, 0};
  BulletType type = PLAYER_BULLET;
  bool active = false;

  void update(float dt);
  void draw() const;
};
