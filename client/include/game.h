#pragma once
#include "constants.h"
#include "objects/player.h"
#include "objects/bullet.h"
#include "objects/enemy.h"
#include "objects/boss.h"

class Game {
public:
  void run();

private:
  void update(float dt);
  void draw();

  Player player;
  Bullet bullets[MAX_BULLETS];
  Enemy  enemies[ENEMY_ROWS * ENEMY_COLS];
  Boss   boss;
};
