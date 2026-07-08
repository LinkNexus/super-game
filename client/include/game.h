#pragma once

#include "constants.h"
#include "objects/boss.h"
#include "objects/bullet.h"
#include "objects/enemy.h"
#include "objects/player.h"
#include "objects/star.h"
#include <array>

class Game {
public:
  void run();

private:
  void update(float dt);
  void draw();

  void initEnemies();
  void updateEnemies(float dt);
  void checkCollisions();

  Player player;
  std::array<Bullet, MAX_BULLETS> bullets;
  std::array<Enemy, ENEMIES_ROWS * ENEMIES_COLS> enemies;
  std::array<Star, STAR_COUNT> stars;
  Boss boss;

  int enemies_direction_ = 1;
};
