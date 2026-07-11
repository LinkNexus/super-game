#pragma once

#include "constants.h"
#include "entities/bullet.h"
#include "raylib.h"
#include <array>

struct Player {
  Vector2 position = {0, 0};
  float fire_cooldown = 0.0f;
  Texture2D texture = {};

  void update(float dt, std::array<Bullet, MAX_BULLETS> &bullets,
              bool can_fire_bullets);
  void loadTexture();
  void unload();
  void draw() const;

  static constexpr float SIZE = 20.0f;
  static constexpr float SPEED = 300.0f;
  static constexpr float HITBOX_SCALE = 0.80f;

private:
  void spawnBullet(std::array<Bullet, MAX_BULLETS> &bullets) const;
};
