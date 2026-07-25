#pragma once

#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/sim/enemy_sim.h"
#include <cstdint>
#include <vector>

namespace shared {

enum Button : uint8_t {
  BUTTON_NONE = 0,
  BUTTON_LEFT = 1 << 0,
  BUTTON_RIGHT = 1 << 1,
  BUTTON_SHOOT = 1 << 2,
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
  uint8_t id;
};

struct BulletState {
  Vec2D position;
  uint8_t type;
};

struct BossState {
  Vec2D position;
  uint8_t active;
  uint32_t health;
};

struct GameState {
  uint32_t tick;
  uint8_t phase;
  std::array<PlayerState, MAX_PLAYERS> players;
  std::vector<BulletState> active_bullets;
  std::array<std::array<uint8_t, 2>,
             EnemiesPoolSimState::COLS * EnemiesPoolSimState::ROWS>
      enemies;
  float enemies_offset_x, enemies_offset_y;
  BossState boss;
};

} // namespace shared
