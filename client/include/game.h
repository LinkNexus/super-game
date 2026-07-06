#pragma once

#include "constants.h"
#include "objects/boss.h"
#include "objects/bullet.h"
#include "objects/enemy.h"
#include "objects/player.h"
#include <array>

class Game {
public:
  void run();

private:
  void update(float dt);
  void draw();

  void spawnBullet();
  void initEnemies();
  void updateEnemies(float dt);

  Player player;
  Bullet bullets[MAX_BULLETS];
  Enemy enemies[ENEMY_ROWS * ENEMY_COLS];
  Boss boss;

  float fire_cooldown_ = 0.0f;
  int enemies_direction_ = 1;
  std::array<float, ENEMY_COLS * ENEMY_ROWS> enemies_shooting_cooldowns_;

  static constexpr float ENEMIES_SPACING_X = 30.0f;
  static constexpr float ENEMIES_SPACING_Y = 30.0f;
  static constexpr float ENEMIES_CYCLE_SPEED = 200.0f;
  static constexpr float ENEMIES_DESCENT_SPEED = 20.0f;
};
