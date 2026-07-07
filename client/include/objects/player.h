#pragma once
#include "constants.h"
#include "objects/bullet.h"
#include "raylib.h"
#include <array>

struct Player {
  Vector2 position = {0, 0};
  float speed = 300.0f;
  float size = 20.0f;
  float fire_cooldown = 0.0f;

  void update(float dt, std::array<Bullet, MAX_BULLETS> &bullets);
  void draw() const;

private:
  void spawnBullet(std::array<Bullet, MAX_BULLETS> &bullets) const;
};
