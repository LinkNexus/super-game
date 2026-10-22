#pragma once

#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/wire.h"
#include "types.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

struct CoopGameType {};
struct PvPGameType {
  shared::PlayerCount team_size{};
  shared::PlayerCount team_count{};
};
using GameType = std::variant<CoopGameType, PvPGameType>;

using TeamId = shared::PvPGameSim::TeamId;

struct PlayerConnection {
  ServerPlayerId id{};
  std::optional<TeamId> team_id{};
  bool is_ready{false};
  /// Tracks whether SHOOT was already held last message, so held-down fire
  /// only counts as one shot per press rather than one per message.
  bool prev_shoot_held{false};
  /// Shot count accumulated since the last `Game::update()` tick, so a
  /// shoot press isn't missed even if it happens between ticks.
  uint8_t pending_shots{};
  shared::Button pending_movement{shared::Button::BUTTON_NONE};
  WsConnection *ws{};
  std::string name{};
};

class Game {
public:
  enum class ClientMessageResult : uint8_t {
    OK,
    NotJson,
    BadEnvelope,
    UnknownType
  };

  struct ClientMessageOutcome {
    ClientMessageResult result{ClientMessageResult::OK};
    std::string detail{};

    explicit operator bool() const { return result == ClientMessageResult::OK; }
  };

  using Players =
      std::array<std::optional<PlayerConnection>, shared::MAX_PLAYERS_PER_GAME>;

  GameId id{};

public:
  virtual ~Game() = default;

  /// Steps the simulation by @p dt using each player's accumulated
  /// pending input, then broadcasts the resulting `GameState` to every
  /// connected player. No-op until `tryStart()` has actually started the
  /// match.
  virtual void update(float dt) = 0;

  /// Assigns @p player to the first free slot, if the match isn't already
  /// full.
  virtual bool addPlayers(const PerSocketPlayers &players_data) = 0;

  /// Frees the slot belonging to @p ws, if present.
  void removePlayers(const WsConnection *ws);

  const Players &getPlayers() const;

  bool isOver() const;

  /// Updates @p playerId's ready flag and attempts to start the match
  /// (see `tryStart()`).
  void setPlayersReady(const WsConnection *ws, bool ready);

  bool isRunning() const;

  const std::vector<WsConnection *> &getPlayerSockets() const;

  bool isEmpty() const;

  virtual bool thereIsEnoughPlayers() const = 0;

  PlayerConnection *findPlayerById(ServerPlayerId id);

  virtual shared::LobbyUpdate buildLobbyUpdate() const = 0;

  void broadcastLobbyUpdate() const;

  Game::ClientMessageOutcome
  handleClientMessage(const PerSocketPlayers &players_data,
                      std::string_view message);

protected:
  bool canStart() const;

  virtual void tryStart() = 0;

  template <class Payload> void broadcast(const Payload &payload) const {
    nlohmann::json envelope{};
    envelope["type"] = Payload::TYPE;
    envelope["payload"] = payload;
    const auto frame = shared::encode(envelope);

    for (auto *ws : player_sockets_) {
      if (ws)
        ws->send({reinterpret_cast<const char *>(frame.data()), frame.size()});
    }
  }

  void insertPlayers(const PerSocketPlayers &player, WsConnection *ws,
                     const std::optional<std::pair<std::size_t, TeamId>>
                         &teamId = std::nullopt);

private:
  bool allPlayersReady() const;

protected:
  bool is_running_{false};
  bool is_over_{false};
  std::vector<WsConnection *> player_sockets_{};
  Players players_{};
  shared::GameSim sim_;
  shared::GameState state_{};
  shared::PlayerInputs<shared::MAX_PLAYERS_PER_GAME> inputs_{};
};

class CoopGame : public Game {
private:
  void tryStart() override;

public:
  void update(float dt) override;

  bool addPlayers(const PerSocketPlayers &players_data) override;

  bool thereIsEnoughPlayers() const override;

  shared::LobbyUpdate buildLobbyUpdate() const override;
};

/// Inverses: `insertPlayers` places a team inside `teamSlotRange`, `rankInTeam`
/// recovers the position. The rank holds only while that placement does.
inline auto teamSlotRange(std::size_t teamIdx) {
  return std::pair{teamIdx * shared::MAX_PLAYERS_PER_TEAM,
                   (teamIdx + 1) * shared::MAX_PLAYERS_PER_TEAM};
}

inline auto rankInTeam(std::size_t slot) {
  return slot % shared::MAX_PLAYERS_PER_TEAM;
}

class PvPGame : public Game {
  using Teams = std::array<std::optional<TeamId>, shared::MAX_TEAMS>;

private:
  void tryStart() override;

public:
  PvPGame(shared::PlayerCount team_size, shared::PlayerCount team_count);

  void update(float dt) override;

  bool addPlayers(const PerSocketPlayers &players_data) override;

  bool thereIsEnoughPlayers() const override;

  shared::LobbyUpdate buildLobbyUpdate() const override;

  const Teams &getTeamIds() const;

private:
  shared::PlayerCount team_size_{};
  shared::PlayerCount team_count_{};
  Teams team_ids_{};

private:
  // forEachPlayerOfTeam's slot only works because insertPlayers places team
  // members in that team's contiguous slots.

  template <typename Fn>
    requires(std::is_invocable_v<Fn, PlayerConnection &, std::size_t>)
  void forEachPlayerOfTeam(TeamId id, Fn callback) {
    shared::iterateOverOptionals(
        players_, [&](PlayerConnection &player, std::size_t slot) {
          if (player.team_id == id) {
            callback(player, rankInTeam(slot));
          }
        });
  }

  template <typename Fn>
    requires(std::is_invocable_v<Fn, const PlayerConnection &, std::size_t>)
  void forEachPlayerOfTeam(TeamId id, Fn callback) const {
    shared::iterateOverOptionals(
        players_, [&](const PlayerConnection &player, std::size_t slot) {
          if (player.team_id == id) {
            callback(player, rankInTeam(slot));
          }
        });
  }
};
