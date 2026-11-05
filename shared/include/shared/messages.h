#pragma once

#include "mathlib.hpp"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/types.h"
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

namespace mathlib {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vec2f, x, y);
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vec3f, x, y, z);
} // namespace mathlib

/// nlohmann::json glue so `std::optional<T>` fields serialize as either the
/// wrapped value or JSON `null`, used throughout the wire structs below for
/// sparsely-populated player slots.
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

template <typename E>
  requires std::is_enum_v<E>
void to_json(nlohmann::json &j, E value) {
  j = static_cast<std::underlying_type_t<E>>(value);
}

template <typename E>
  requires std::is_enum_v<E>
void from_json(const nlohmann::json &j, E &value) {
  value = static_cast<E>(j.get<std::underlying_type_t<E>>());
}
} // namespace nlohmann

namespace shared {
enum Button : uint8_t {
  BUTTON_NONE = 0,
  BUTTON_LEFT = 1 << 0,
  BUTTON_RIGHT = 1 << 1,
  BUTTON_SHOOT = 1 << 2,
};

struct PlayerInput {
  Button buttons{};
  PlayerId player_id{};
};
template <std::size_t N>
using PlayerInputs = std::array<std::optional<PlayerInput>, N>;

enum class ServerMessageType : uint8_t {
  WELCOME,
  COOP_GAME_LOBBY_UPDATE,
  PVP_GAME_LOBBY_UPDATE,
  PVP_GAME_STATE,
  COOP_GAME_STATE,
};

enum class ClientMessageType : uint8_t { PLAYER_INPUT, READY, SWITCH_TEAM };

constexpr bool isClientMessageTypeValid(ClientMessageType type) {
  switch (type) {
  case ClientMessageType::PLAYER_INPUT:
  case ClientMessageType::READY:
  case ClientMessageType::SWITCH_TEAM:
    return true;
  }
  return false;
}

/// Sent once by the server right after a WebSocket connection completes,
/// telling the client which player id it has been assigned.
struct WelcomeMessage {
  PlayerIds<MAX_PLAYERS_PER_CLIENT> players_ids{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WelcomeMessage, players_ids);

/// Client-to-server message toggling the sender's ready state while waiting
/// in the lobby. A match only starts once every connected player is ready.
struct ReadyMessage {
  PlayerId player_id{};
  bool is_ready{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ReadyMessage, is_ready, player_id)

struct SwitchTeamMessage {
  PlayerId player_id{};
  TeamId target_team_id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(SwitchTeamMessage, player_id, target_team_id)

/// Per-player identity/lobby-state snapshot broadcast as part of
/// LobbyUpdate - name and ready status only, no gameplay state.
struct PlayerInfo {
  PlayerId id{};
  std::string name{};
  bool is_ready{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerInfo, name, is_ready, id)

struct CoopGameLobbyUpdate {
  using PlayerInfos = std::array<std::optional<PlayerInfo>, MAX_PLAYERS_COOP>;

  PlayerInfos players{};
  bool game_started{};

  static constexpr auto TYPE = ServerMessageType::COOP_GAME_LOBBY_UPDATE;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CoopGameLobbyUpdate, players, game_started)

struct TeamInfo {
  TeamId id{};
  std::array<std::optional<PlayerInfo>, MAX_PLAYERS_PER_TEAM> players{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TeamInfo, id, players)

struct PvPGameLobbyUpdate {
  using TeamInfos = std::array<std::optional<TeamInfo>, MAX_TEAMS>;

  TeamInfos teams{};
  PlayerCount team_size{};
  bool game_started{};

  static constexpr auto TYPE = ServerMessageType::PVP_GAME_LOBBY_UPDATE;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PvPGameLobbyUpdate, teams, game_started,
                                   team_size)

using LobbyUpdate = std::variant<CoopGameLobbyUpdate, PvPGameLobbyUpdate>;

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerInput, buttons, player_id)

struct PlayerState {
  mathlib::Vec2f position{};
  PlayerLives lives{};
  PlayerPoints points{};
  float orientation{};
  std::string name{};
  PlayerId id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerState, position, lives, points, id,
                                   name, orientation)

/// Trimmed bullet state for rendering; `type` mirrors `BulletType` and
/// `active` marks pool slots currently in flight.
struct BulletState {
  mathlib::Vec2f position{};
  BulletType type{};
  bool active{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BulletState, position, type, active)

using BulletsPoolState = std::array<BulletState, MAX_BULLETS>;

struct BossState {
  mathlib::Vec2f position{};
  bool active{};
  BossHealth health{};
  BossHealth max_health{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BossState, position, active, health,
                                   max_health)

using EnemiesPoolState =
    std::array<std::pair<bool, EnemyType>, ENEMY_COLS * MAX_ENEMY_ROWS>;

struct CommonGameState {
  BulletsPoolState bullets{};
};

struct CoopGameState : CommonGameState {
  CoopGamePhase phase{};
  std::array<std::optional<PlayerState>, MAX_PLAYERS_COOP> players{};
  EnemiesPoolState enemies{};
  mathlib::Vec2f enemies_offset{};
  BossState boss{};

  static constexpr auto TYPE = ServerMessageType::COOP_GAME_STATE;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CoopGameState, phase, players, bullets,
                                   enemies, enemies_offset, boss)

struct TeamState {
  TeamId id{};
  TeamOutcome outcome{};
  std::array<std::optional<PlayerState>, MAX_PLAYERS_PER_TEAM> players{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TeamState, id, outcome, players)

struct PvPGameState : CommonGameState {
  PvPGamePhase phase{};
  std::array<std::optional<TeamState>, MAX_TEAMS> teams{};

  static constexpr auto TYPE = ServerMessageType::PVP_GAME_STATE;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PvPGameState, phase, teams, bullets)

using GameState = std::variant<CoopGameState, PvPGameState>;
} // namespace shared
