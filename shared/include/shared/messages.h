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

/// Sent once by the server right after a WebSocket connection completes,
/// telling the client which player id it has been assigned.
struct WelcomeMessage {
  std::array<PlayerId, MAX_PLAYERS_PER_CLIENT> players_ids{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WelcomeMessage, players_ids);

/// Maximum length of a player-chosen display name, in characters (excludes
/// the null terminator on the server's fixed-size storage buffer).
constexpr std::size_t MAX_NAME_LENGTH = 9;

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
  std::array<PlayerInfo, MAX_PLAYERS_COOP> players{};
  shared::PlayerCount players_count{};
  shared::PlayerCount max_players_count{};
  bool game_started{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CoopGameLobbyUpdate, players, game_started,
                                   max_players_count, players_count)

struct PvPGameLobbyUpdate {
  struct Team {
    std::array<PlayerInfo, MAX_PLAYERS_PER_TEAM> players{};
    shared::PlayerCount players_count{};
  };

  std::array<Team, MAX_TEAMS> teams{};
  uint8_t team_count{};
  uint8_t team_size{};
  bool game_started{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PvPGameLobbyUpdate::Team, players,
                                   players_count)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PvPGameLobbyUpdate, teams, game_started,
                                   team_size, team_count)

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
  std::array<PlayerState, MAX_PLAYERS_COOP> players{};
  uint8_t player_count{};
  EnemiesPoolState enemies{};
  float enemies_offset_x, enemies_offset_y{};
  BossState boss{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CoopGameState, phase, players, bullets,
                                   enemies, enemies_offset_x, enemies_offset_y,
                                   boss, player_count)

struct TeamState {
  uint8_t id{};
  uint8_t outcome{};
  std::array<PlayerState, MAX_PLAYERS_PER_TEAM> players{};
  uint8_t size{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TeamState, id, outcome, players, size)

struct PvPGameState : BaseState {
  std::array<TeamState, MAX_TEAMS> teams{};
  uint8_t teams_count{};
  uint8_t team_size{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PvPGameState, phase, teams, bullets,
                                   teams_count, team_size)

using GameState = std::variant<CoopGameState, PvPGameState>;

} // namespace shared
