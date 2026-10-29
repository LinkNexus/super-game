#pragma once

#include "nlohmann/json.hpp"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/sim/enemy_sim.h"
#include <cstdint>
#include <optional>
#include <variant>

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
} // namespace nlohmann

namespace shared {

/// Movement/shoot input bitmask carried by PlayerInput. Bitwise-OR'd, not
/// mutually exclusive.
enum Button : uint8_t {
  BUTTON_NONE = 0,
  BUTTON_LEFT = 1 << 0,
  BUTTON_RIGHT = 1 << 1,
  BUTTON_SHOOT = 1 << 2,
};

/// Envelope discriminator for server-to-client messages (see the `"type"`
/// key in the `{type, payload}` JSON envelope sent by the server).
enum class ServerMessageType : uint8_t {
  WELCOME,
  COOP_GAME_LOBBY_UPDATE,
  PVP_GAME_LOBBY_UPDATE,
  PVP_GAME_STATE,
  COOP_GAME_STATE,
};

/// Envelope discriminator for client-to-server messages.
enum class ClientMessageType : uint8_t { PLAYER_INPUT, READY };

constexpr bool isClientMessageTypeValid(ClientMessageType type) {
  switch (type) {
  case ClientMessageType::PLAYER_INPUT:
  case ClientMessageType::READY:
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

/// Maximum length of a player-chosen display name, in characters (excludes
/// the null terminator on the server's fixed-size storage buffer).
constexpr std::size_t MAX_NAME_LENGTH = 9;

static inline auto isNameValid(const std::string_view name) {
  for (const auto &c : name) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'))
      return false;
  }

  return name.size() <= MAX_NAME_LENGTH;
}

/// Client-to-server message toggling the sender's ready state while waiting
/// in the lobby. A match only starts once every connected player is ready.
struct ReadyMessage {
  bool is_ready{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ReadyMessage, is_ready)

/// Per-player identity/lobby-state snapshot broadcast as part of
/// LobbyUpdate - name and ready status only, no gameplay state.
struct PlayerInfo {
  std::uint32_t id{};
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

struct PvPGameLobbyUpdate {
  using TeamInfos =
      std::array<std::optional<std::array<std::optional<PlayerInfo>,
                                          MAX_PLAYERS_PER_TEAM>>,
                 MAX_TEAMS>;

  TeamInfos teams{};
  uint8_t team_size{};
  bool game_started{};

  static constexpr auto TYPE = ServerMessageType::PVP_GAME_LOBBY_UPDATE;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PvPGameLobbyUpdate, teams, game_started,
                                   team_size)

using LobbyUpdate = std::variant<CoopGameLobbyUpdate, PvPGameLobbyUpdate>;

/// Per-tick movement/shoot input, sent by the client during `PLAYING` and
/// consumed by GameSim::step(). `player_id` is only trusted for local
/// multi-player input routing (which of the two local players an input
/// belongs to) - the server always attributes online input to the
/// connection it arrived on, never to this field.
struct PlayerInput {
  uint8_t buttons{};
  PlayerId player_id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerInput, buttons, player_id)

template <std::size_t N>
using PlayerInputs = std::array<std::optional<PlayerInput>, N>;

/// Trimmed per-player state that crosses the wire each tick, as opposed to
/// the full internal `PlayerSimState` (which also tracks fire cooldowns
/// etc. and never leaves the simulation process).
struct PlayerState {
  Vec2D position{};
  uint8_t lives{};
  uint32_t points{};
  float orientation{};
  std::string name{};
  PlayerId id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerState, position, lives, points, id,
                                   name, orientation)

/// Trimmed bullet state for rendering; `type` mirrors `BulletType` and
/// `active` marks pool slots currently in flight.
struct BulletState {
  Vec2D position{};
  uint8_t type{};
  uint8_t active{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BulletState, position, type, active)

using BulletsPoolState = std::array<BulletState, MAX_BULLETS>;

/// Trimmed boss state for rendering, including `max_health` so the client
/// can compute the health bar's fill ratio without hardcoding a constant
/// that would be wrong once boss health scales with player count.
struct BossState {
  Vec2D position{};
  uint8_t active{};
  uint32_t health{};
  uint32_t max_health{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BossState, position, active, health,
                                   max_health)

using EnemiesPoolState =
    std::array<std::array<uint8_t, 2>,
               EnemiesPoolSimState::COLS * EnemiesPoolSimState::MAX_ROWS>;

struct BaseState {
  uint8_t phase{};
  BulletsPoolState bullets{};
};

struct CoopGameState : BaseState {
  std::array<std::optional<PlayerState>, MAX_PLAYERS_COOP> players{};
  EnemiesPoolState enemies{};
  float enemies_offset_x, enemies_offset_y{};
  BossState boss{};

  static constexpr auto TYPE = ServerMessageType::COOP_GAME_STATE;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CoopGameState, phase, players, bullets,
                                   enemies, enemies_offset_x, enemies_offset_y,
                                   boss)

struct TeamState {
  uint8_t id{};
  uint8_t outcome{};
  std::array<std::optional<PlayerState>, MAX_PLAYERS_PER_TEAM> players{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TeamState, id, outcome, players)

struct PvPGameState : BaseState {
  std::array<std::optional<TeamState>, MAX_TEAMS> teams{};

  static constexpr auto TYPE = ServerMessageType::PVP_GAME_STATE;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PvPGameState, phase, teams, bullets)

using GameState = std::variant<CoopGameState, PvPGameState>;

} // namespace shared
