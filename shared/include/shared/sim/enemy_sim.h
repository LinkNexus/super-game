#pragma once

#include "shared/constants.h"
#include "shared/sim/bullet_sim.h"
#include <array>
#include <cstddef>
#include <cstdint>

namespace shared {
enum class EnemyType : uint8_t {
  TYPE_1,
  TYPE_2,
};

struct EnemySimState {
  Vec2D position;
  EnemyType type;
  bool alive = false;
  float shooting_cooldown = 0;

  static constexpr float WIDTH = 32.0f;
  static constexpr float HEIGHT = 20.0f;
  static constexpr float CYCLE_SPEED = 200.0f;
  static constexpr float DESCENT_SPEED = 35.0f;
  static constexpr float SHOOTING_INTERVAL_MIN = 1.5f;
  static constexpr float SHOOTING_INTERVAL_MAX = 3.5f;

  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;
};

struct EnemiesPoolSimState {
  static constexpr float INITIAL_OFFSET_Y = -270.0f;
  static constexpr float FINAL_OFFSET_Y = 50.0f;
  static constexpr float INITIAL_DESCENT_SPEED = 70.0f;
  static constexpr int ROWS = 5;
  static constexpr int COLS = 10;
  static constexpr float SPACING_X = 40.0f;
  static constexpr float SPACING_Y = 40.0f;

  std::array<EnemySimState, ROWS * COLS> enemies;
  std::size_t alive_enemies_count;
  float offset_x;
  float offset_y;
  int direction;

  void init();
  void stepEntrance(float dt);
  bool isEntranceComplete();
  void step(float dt, std::array<BulletSimState, MAX_BULLETS> &bullets_pool);
  bool allEnemiesDefeated();
  bool reachedPlayer();

private:
  void stepEnemy(EnemySimState &enemy, std::size_t idx);
};

} // namespace shared
