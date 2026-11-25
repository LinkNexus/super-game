#pragma once

#include "shared/helpers.h"
#include "shared/sim/constants.h"
#include "shared/sim/messages.h"
#include "shared/sim/modes.h"
#include "shared/sim/types.h"
#include "shared/wire.h"
#include "types.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

using namespace shared::sim;

struct CoopGameType {};

struct PvPGameType {
  PlayerCount team_size{};
  TeamCount team_count{};
};

using GameType = std::variant<CoopGameType, PvPGameType>;

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
  Button pending_movement{Button::NONE};
  WsConnection *ws{};
  std::string name{};
};

class Game {
public:
  enum class ClientMessageResult : uint8_t { OK, NotJson, BadEnvelope, UnknownType };

  struct ClientMessageOutcome {
    ClientMessageResult result{ClientMessageResult::OK};
    std::string detail{};

    explicit operator bool() const { return result == ClientMessageResult::OK; }
  };

  using Players = std::array<std::optional<PlayerConnection>, MAX_PLAYERS_PER_GAME>;

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

  void setPlayerReady(ServerPlayerId id, bool ready, WsConnection *ws);

  bool isRunning() const;

  const std::vector<WsConnection *> &getPlayerSockets() const;

  bool isEmpty() const;

  virtual bool thereIsEnoughPlayers() const = 0;

  PlayerConnection *findPlayerById(ServerPlayerId id);

  virtual LobbyUpdate buildLobbyUpdate() const = 0;

  void broadcastLobbyUpdate() const;

  Game::ClientMessageOutcome
  handleClientMessage(const PerSocketPlayers &players_data, std::string_view message);

protected:
  bool canStart() const;

  virtual void tryStart() = 0;

  template <class Payload>
  void broadcast(const Payload &payload) const {
    nlohmann::json envelope{};
    envelope["type"] = Payload::TYPE;
    envelope["payload"] = payload;

    for (auto *ws : player_sockets_) {
      if (ws) {
        const auto frame = shared::encode(envelope, ws->format());
        ws->send({reinterpret_cast<const char *>(frame.data()), frame.size()});
      }
    }
  }

  void insertPlayer(
      const std::pair<ServerPlayerId, std::string> &player,
      WsConnection *ws,
      const std::optional<std::pair<std::size_t, TeamId>> &teamId = std::nullopt
  );

private:
  bool allPlayersReady() const;

protected:
  bool is_running_{false};
  bool is_over_{false};
  std::vector<WsConnection *> player_sockets_{};
  Players players_{};
  GameSim sim_;
  GameState state_{};
  PlayerInputs<MAX_PLAYERS_PER_GAME> inputs_{};
};

class CoopGame : public Game {
private:
  void tryStart() override;

public:
  void update(float dt) override;

  bool addPlayers(const PerSocketPlayers &players_data) override;

  bool thereIsEnoughPlayers() const override;

  LobbyUpdate buildLobbyUpdate() const override;
};

/// Inverses: `insertPlayers` places a team inside `teamSlotRange`, `rankInTeam`
/// recovers the position. The rank holds only while that placement does.
inline auto teamSlotRange(std::size_t teamIdx) {
  return std::pair{teamIdx * MAX_PLAYERS_PER_TEAM, (teamIdx + 1) * MAX_PLAYERS_PER_TEAM};
}

inline auto rankInTeam(std::size_t slot) {
  return slot % MAX_PLAYERS_PER_TEAM;
}

class PvPGame : public Game {
  using Teams = std::array<std::optional<TeamId>, MAX_TEAMS>;

private:
  void tryStart() override;

public:
  PvPGame(PlayerCount team_size, PlayerCount team_count);

  void update(float dt) override;

  bool addPlayers(const PerSocketPlayers &players_data) override;

  bool thereIsEnoughPlayers() const override;

  LobbyUpdate buildLobbyUpdate() const override;

  const Teams &getTeamIds() const;

  std::size_t countPlayersOfTeam(TeamId id) const;

  PlayerCount getTeamSize() const;

private:
  PlayerCount team_size_{};
  TeamCount team_count_{};
  Teams team_ids_{};

public:
  // forEachPlayerOfTeam's slot only works because insertPlayers places team
  // members in that team's contiguous slots.

  template <typename Fn>
    requires(std::is_invocable_v<Fn, PlayerConnection &, std::size_t>)
  void forEachPlayerOfTeam(TeamId id, Fn callback) {
    shared::iterateOverOptionals(players_, [&](PlayerConnection &player, std::size_t slot) {
      if (player.team_id == id) {
        callback(player, rankInTeam(slot));
      }
    });
  }

  template <typename Fn>
    requires(std::is_invocable_v<Fn, const PlayerConnection &, std::size_t>)
  void forEachPlayerOfTeam(TeamId id, Fn callback) const {
    shared::iterateOverOptionals(players_, [&](const PlayerConnection &player, std::size_t slot) {
      if (player.team_id == id) {
        callback(player, rankInTeam(slot));
      }
    });
  }
};
