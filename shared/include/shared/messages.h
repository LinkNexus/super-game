#pragma once

#include "shared/constants.h"
#include "shared/math_utils.h"
#include <cstdint>

namespace shared {

enum Button : uint8_t {
  BUTTON_LEFT = 1 << 0,
  BUTTON_RIGHT = 1 << 1,
  BUTTON_FIRE = 1 << 2,
  BUTTON_PAUSE = 1 << 3,
};

struct PlayerInput {
  uint32_t tick;
  uint8_t buttons;
  uint8_t player_id;
};

struct PlayerState {
  Vec2D position;
  uint8_t lives;
  uint32_t points;
};

struct BulletState {
  Vec2D position;
  uint8_t active;
  uint8_t type;
};

struct BossState {
  Vec2D position;
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
