#pragma once

#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/sim/enemy_sim.h"
#include <cstdint>

namespace shared {

enum Button : uint8_t {
  BUTTON_NONE = 0,
  BUTTON_LEFT = 1 << 0,
  BUTTON_RIGHT = 1 << 1,
  BUTTON_SHOOT = 1 << 2,
};

enum class MessageType : uint8_t {
  LOBBY_UPDATE,
  GAME_STATE,
};

struct LobbyUpdate {
  uint8_t player_count;
  bool game_started;
  uint8_t max_players;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LobbyUpdate, player_count, game_started,
                                   max_players)

struct PlayerInput {
  uint32_t tick;
  uint8_t buttons;
  uint32_t player_id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerInput, tick, buttons, player_id)

struct PlayerState {
  Vec2D position;
  uint8_t lives;
  uint32_t points;
  uint8_t id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerState, position, lives, points, id)

struct BulletState {
  Vec2D position;
  uint8_t type;
  uint8_t active;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BulletState, position, type, active)

struct BossState {
  Vec2D position;
  uint8_t active;
  uint32_t health;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BossState, position, active, health)

struct GameState {
  uint32_t tick;
  uint8_t phase;
  std::array<PlayerState, MAX_PLAYERS> players;
  std::array<BulletState, MAX_BULLETS> bullets;
  std::array<std::array<uint8_t, 2>,
             EnemiesPoolSimState::COLS * EnemiesPoolSimState::ROWS>
      enemies;
  float enemies_offset_x, enemies_offset_y;
  BossState boss;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GameState, tick, phase, players, bullets,
                                   enemies, enemies_offset_x, enemies_offset_y,
                                   boss)

} // namespace shared
