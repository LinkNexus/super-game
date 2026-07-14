#pragma once

#include "constants.h"
#include "entities/boss.h"
#include "entities/bullet.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/star.h"
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
  void initBoss();
  void animateBossEntrance(float dt);
  void updateBoss(float dt);

  Player player_;
  std::array<Bullet, MAX_BULLETS> bullets_;
  std::array<Enemy, ENEMIES_ROWS * ENEMIES_COLS> enemies_;
  std::array<Star, STAR_COUNT> stars_;
  Boss boss_;

  int enemies_direction_ = 1;

  bool boss_phase_ = false;
  bool boss_entrance_running_ = false;
};
