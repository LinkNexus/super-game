#pragma once

#include "constants.h"
#include "entities/boss.h"
#include "entities/bullet.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/star.h"
#include "shared/messages.h"
#include <array>

enum class Screen { MENU, PLAYING, PAUSED, GAME_OVER, WIN };

class Game {
public:
  void run();

private:
  void init();
  void update(float dt);
  void draw();

  void handleInput();
  void updatePlaying(float dt);

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
  Screen screen_;

  int enemies_direction_;

  bool boss_phase_;
  bool boss_entrance_running_;
};
