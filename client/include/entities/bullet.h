#pragma once
#include "raylib.h"

enum class BulletType { PLAYER, ENEMY };

struct Bullet {
  Vector2 position = {0, 0};
  Vector2 velocity = {0, 0};
  BulletType type = BulletType::PLAYER;
  bool active = false;

  void update(float dt);
  void draw() const;

  static constexpr float WIDTH = 4;
  static constexpr float HEIGHT = 12;

  static constexpr float SPEED = 600.0f;
};
