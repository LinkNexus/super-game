#pragma once

#include "entities/bullet.h"
#include "raylib.h"
#include "shared/constants.h"
#include <array>

struct Player {
  Vector2 position = {0, 0};
  float fire_cooldown = 0.0f;
  Texture2D texture = {};
  int lives = INITIAL_LIVES;
  int points = 0;

  void update(float dt, std::array<Bullet, MAX_BULLETS> &bullets,
              bool can_fire_bullets);
  void loadTexture();
  void unload();
  void draw() const;

  static constexpr float SIZE = 20.0f;
  static constexpr float SPEED = 300.0f;
  static constexpr float HITBOX_SCALE = 0.80f;
  static constexpr float FIRE_COOLDOWN = 0.15f;
  static constexpr int INITIAL_LIVES = 3;

private:
  void spawnBullet(std::array<Bullet, MAX_BULLETS> &bullets) const;
};
