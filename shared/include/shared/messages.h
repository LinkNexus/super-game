#pragma once

#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/sim/enemy_sim.h"
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace nlohmann {
template <typename T> struct adl_serializer<std::optional<T>> {
  static void to_json(json &j, const std::optional<T> &opt) {
    if (opt)
      j = *opt;
    else
      j = nullptr;
  }

  static void from_json(const json &j, std::optional<T> &opt) {
    if (j.is_null())
      opt = std::nullopt;
    else
      opt = j.template get<T>();
  }
};
} // namespace nlohmann

namespace shared {

enum Button : uint8_t {
  BUTTON_NONE = 0,
  BUTTON_LEFT = 1 << 0,
  BUTTON_RIGHT = 1 << 1,
  BUTTON_SHOOT = 1 << 2,
};

enum class MessageType : uint8_t {
  WELCOME,
  LOBBY_UPDATE,
  GAME_STATE,
};

struct WelcomeMessage {
  uint32_t player_id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WelcomeMessage, player_id);

constexpr std::size_t MAX_NAME_LENGTH = 9;

struct PlayerInfo {
  std::string name{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerInfo, name)

struct LobbyUpdate {
  std::array<std::optional<PlayerInfo>, MAX_PLAYERS> players{};
  uint8_t player_count{};
  bool game_started{};
  uint8_t max_players{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LobbyUpdate, players, player_count,
                                   game_started, max_players)

struct PlayerInput {
  uint8_t buttons{};
  uint32_t player_id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerInput, buttons, player_id)

struct PlayerState {
  Vec2D position{};
  uint8_t lives{};
  uint32_t points{};
  std::string name{};
  uint8_t id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerState, position, lives, points, id,
                                   name)

struct BulletState {
  Vec2D position{};
  uint8_t type{};
  uint8_t active{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BulletState, position, type, active)

struct BossState {
  Vec2D position{};
  uint8_t active{};
  uint32_t health{};
  uint32_t max_health{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BossState, position, active, health,
                                   max_health)

struct GameState {
  uint8_t phase{};
  std::array<std::optional<PlayerState>, MAX_PLAYERS> players{};
  std::array<BulletState, MAX_BULLETS> bullets{};
  std::array<std::array<uint8_t, 2>,
             EnemiesPoolSimState::COLS * EnemiesPoolSimState::MAX_ROWS>
      enemies{};
  float enemies_offset_x, enemies_offset_y{};
  BossState boss{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GameState, phase, players, bullets, enemies,
                                   enemies_offset_x, enemies_offset_y, boss)

} // namespace shared
