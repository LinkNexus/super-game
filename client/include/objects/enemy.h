#pragma once

#include "bullet.h"
#include "constants.h"
#include "raylib.h"
#include <array>

enum class EnemyType { TYPE_1, TYPE_2 };

struct Enemy {
  Vector2 position = {0, 0};
  EnemyType type = EnemyType::TYPE_1;
  bool alive = false;
  float shooting_cooldown = 0;

  void draw() const;
  void spawnBullet(std::array<Bullet, MAX_BULLETS> &bullets) const;

  static constexpr float WIDTH = 32.0f;
  static constexpr float HEIGHT = 20.0f;

  static void loadTextures();
  static void unloadTextures();

  private:
  static Texture2D texture_type1_;
  static Texture2D texture_type2_;
};
