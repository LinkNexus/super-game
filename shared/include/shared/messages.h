#pragma once

#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/sim/enemy_sim.h"
#include <cstdint>
#include <optional>

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
  LOBBY_UPDATE,
  GAME_STATE,
};

/// Envelope discriminator for client-to-server messages.
enum class ClientMessageType : uint8_t { PLAYER_INPUT, READY };

/// Sent once by the server right after a WebSocket connection completes,
/// telling the client which player id it has been assigned.
struct WelcomeMessage {
  uint32_t player_id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(WelcomeMessage, player_id);

/// Maximum length of a player-chosen display name, in characters (excludes
/// the null terminator on the server's fixed-size storage buffer).
constexpr std::size_t MAX_NAME_LENGTH = 9;

/// Client-to-server message toggling the sender's ready state while waiting
/// in the lobby. A match only starts once every connected player is ready.
struct ReasyMessage {
  bool is_ready{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ReasyMessage, is_ready)

/// Per-player identity/lobby-state snapshot broadcast as part of
/// LobbyUpdate - name and ready status only, no gameplay state.
struct PlayerInfo {
  std::uint32_t id{};
  std::string name{};
  bool is_ready{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerInfo, name, is_ready, id)

/// Broadcast by the server whenever the lobby state changes (a player
/// joins, leaves, or toggles ready). `game_started` becomes true only once
/// every slot is filled and ready, at which point the client transitions
/// from `Screen::LOBBY` to `Screen::PLAYING`.
struct LobbyUpdate {
  std::array<std::optional<PlayerInfo>, MAX_PLAYERS> players{};
  uint8_t player_count{};
  bool game_started{};
  uint8_t max_players{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(LobbyUpdate, players, player_count,
                                   game_started, max_players)

/// Per-tick movement/shoot input, sent by the client during `PLAYING` and
/// consumed by GameSim::step(). `player_id` is only trusted for local
/// multi-player input routing (which of the two local players an input
/// belongs to) - the server always attributes online input to the
/// connection it arrived on, never to this field.
struct PlayerInput {
  uint8_t buttons{};
  uint32_t player_id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerInput, buttons, player_id)

/// Trimmed per-player state that crosses the wire each tick, as opposed to
/// the full internal `PlayerSimState` (which also tracks fire cooldowns
/// etc. and never leaves the simulation process).
struct PlayerState {
  Vec2D position{};
  uint8_t lives{};
  uint32_t points{};
  std::string name{};
  uint8_t id{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerState, position, lives, points, id,
                                   name)

/// Trimmed bullet state for rendering; `type` mirrors `BulletType` and
/// `active` marks pool slots currently in flight.
struct BulletState {
  Vec2D position{};
  uint8_t type{};
  uint8_t active{};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(BulletState, position, type, active)

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

/// The full per-tick snapshot GameSim::step() produces and the server
/// broadcasts to every connected client; the client's local mode consumes
/// the same struct produced in-process. `enemies` packs
/// `{alive, type}` per grid slot rather than sending full per-enemy
/// transforms, since enemy positions are cheaply reconstructed client-side
/// from `enemies_offset_x/y` plus the slot index.
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
