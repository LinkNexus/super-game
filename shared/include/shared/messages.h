#pragma once

#include "shared/constants.h"
#include <cstdint>

namespace shared {

enum class Button : uint8_t {
  LEFT = 1 << 0,
  RIGHT = 1 << 1,
  FIRE = 1 << 2,
};

struct PlayerInput {
  uint32_t tick;
  uint8_t buttons;
  uint8_t player_id;
};

struct PlayerState {
  float x, y;
  uint8_t lives;
  uint32_t points;
};

struct BulletState {
  float x, y;
  uint8_t active;
  uint8_t type;
};

struct BossState {
  float x, y;
  uint8_t active;
  uint32_t health;
};

struct GameState {
  uint32_t tick;
  uint8_t screen;
  PlayerState players[2];
  BulletState bullets[MAX_BULLETS];
  uint8_t enemies_alive[ENEMIES_COLS * ENEMIES_ROWS];
  float enemies_offset_x, enemies_offset_y;
  BossState boss;
};

} // namespace shared
