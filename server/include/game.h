#pragma once

#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
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

struct PlayerConnection {
  /// Shot count accumulated since the last `Game::update()` tick, so a
  /// shoot press isn't missed even if it happens between ticks.
  uint8_t pending_shots{};
  /// Tracks whether SHOOT was already held last message, so held-down fire
  /// only counts as one shot per press rather than one per message.
  bool prev_shoot_held{false};
  bool is_ready{false};
  shared::Button pending_movement{shared::Button::BUTTON_NONE};
  std::string name{};
  ServerPlayerId id{};
  shared::PlayerId id_in_game{};
  WsConnection *ws{};
};

class Game {
public:
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
  virtual void removePlayers(const WsConnection *ws) = 0;

  bool isOver() const;

  /// Updates @p playerId's ready flag and attempts to start the match
  /// (see `tryStart()`).
  virtual void setPlayersReady(const WsConnection *ws, bool ready) = 0;

  virtual bool thereIsEnoughPlayers() const = 0;

  bool isRunning() const;

  const std::vector<WsConnection *> &getPlayerSockets() const;

  bool isEmpty() const;

  bool canStart() const;

  virtual bool allPlayersReady() const = 0;

  virtual PlayerConnection *findPlayerById(ServerPlayerId id) = 0;

protected:
  bool is_running_{false};
  bool is_over_{false};
  std::vector<WsConnection *> player_sockets_{};
};

class CoopGame : public Game {
private:
  void tryStart();
  bool allPlayersReady() const override;

public:
  void update(float dt) override;

  bool thereIsEnoughPlayers() const override;

  const std::array<std::optional<PlayerConnection>, shared::MAX_PLAYERS_COOP> &
  getPlayers() const;

  bool addPlayers(const PerSocketPlayers &players_data) override;

  void removePlayers(const WsConnection *ws) override;

  void setPlayersReady(const WsConnection *ws, bool ready) override;

  PlayerConnection *findPlayerById(ServerPlayerId id) override;

private:
  std::array<std::optional<PlayerConnection>, shared::MAX_PLAYERS_COOP>
      players_{};
  std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS_COOP>
      inputs_{};

  shared::CoopGameSim sim_;
  shared::CoopGameState state_;
};

class PvPGame : public Game {
  struct Team {
    std::array<std::optional<PlayerConnection>, shared::MAX_PLAYERS_PER_TEAM>
        players{};
  };
  using Teams = std::array<Team, shared::MAX_TEAMS>;

private:
  void tryStart();

public:
  PvPGame(shared::PlayerCount team_size, shared::PlayerCount team_count);

  void update(float dt) override;

  bool addPlayers(const PerSocketPlayers &players_data) override;

  void removePlayers(const WsConnection *ws) override;

  bool thereIsEnoughPlayers() const override;

  const Teams &getTeams() const;

  void setPlayersReady(const WsConnection *ws, bool ready) override;

  PlayerConnection *findPlayerById(ServerPlayerId id) override;

private:
  bool allPlayersReady() const override;

private:
  shared::PlayerCount team_size_{};
  shared::PlayerCount team_count_{};
  Teams teams_{};
  std::array<std::optional<shared::PlayerInput>,
             shared::MAX_PLAYERS_PER_TEAM * shared::MAX_TEAMS>
      inputs_{};

  shared::PvPGameSim sim_;
  shared::PvPGameState state_;
};
