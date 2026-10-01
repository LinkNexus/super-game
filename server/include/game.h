#pragma once

#include "WebSocket.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <array>
#include <cstdint>
#include <unordered_map>

class Game;
struct PlayerConnection;
struct PerSocketData;

struct CoopGameType {};
struct PvPGameType {
  shared::PlayerCount team_size{};
  shared::PlayerCount team_count{};
};
using GameType = std::variant<CoopGameType, PvPGameType>;

using WsPtr = uWS::WebSocket<false, true, PerSocketData> *;

/// Per-WebSocket-connection user data (uWebSockets' `ws<PerSocketData>`
/// template parameter). Populated in the `.upgrade` handler before the
/// socket exists, then filled in further in `.open`.
struct PerSocketPlayers {
  std::array<PlayerConnection *, shared::MAX_PLAYERS_PER_CLIENT> players{};
  uint8_t count{};
  WsPtr ws{};
};

struct PerSocketData {
  GameType game_type{};
  Game *game = nullptr;
  PerSocketPlayers players_data{};
};

/// One connected player: identity, name, and the per-tick input state the
/// `.message` handler accumulates between `Game::update()` calls.
struct PlayerConnection {
  uint32_t id{};
  shared::PlayerId id_in_game{};
  WsPtr ws{};
  char name[shared::MAX_NAME_LENGTH + 1]{};
  shared::Button pending_movement{shared::Button::BUTTON_NONE};
  /// Shot count accumulated since the last `Game::update()` tick, so a
  /// shoot press isn't missed even if it happens between ticks.
  uint8_t pending_shots{};
  /// Tracks whether SHOOT was already held last message, so held-down fire
  /// only counts as one shot per press rather than one per message.
  bool prev_shoot_held{false};
  bool is_ready{false};
};

class Game {
public:
  uint32_t id;

public:
  virtual ~Game() = default;

  /// Steps the simulation by @p dt using each player's accumulated
  /// pending input, then broadcasts the resulting `GameState` to every
  /// connected player. No-op until `tryStart()` has actually started the
  /// match.
  virtual void update(float dt) = 0;

  /// Assigns @p player to the first free slot, if the match isn't already
  /// full.
  // virtual bool addPlayer(PlayerConnection *player);
  virtual bool addPlayers(const PerSocketPlayers &players_data) = 0;

  /// Frees the slot belonging to @p playerId, if presen.
  virtual void removePlayers(const WsPtr ws) = 0;

  bool isOver() const;

  /// Updates @p playerId's ready flag and attempts to start the match
  /// (see `tryStart()`).
  virtual void setPlayersReady(WsPtr ws, bool ready) = 0;

  virtual bool isFull() const = 0;

  bool isRunning() const;

  virtual void setPlayerName(shared::PlayerState &state) = 0;

  const std::vector<WsPtr> &getPlayerSockets() const;

protected:
  bool is_running_{false};
  bool is_over_{false};

  std::vector<WsPtr> player_sockets_{};
};

class CoopGame : public Game {
private:
  void tryStart();

public:
  void update(float dt) override;

  bool isFull() const override;

  const std::array<PlayerConnection *, shared::MAX_PLAYERS_COOP> &
  getPlayers() const;

  bool addPlayers(const PerSocketPlayers &players_data) override;

  void removePlayers(const WsPtr ws) override;

  bool allPlayersReady() const;

  void setPlayerName(shared::PlayerState &state) override;

  void setPlayersReady(WsPtr ws, bool ready) override;

  shared::PlayerCount getPlayersCount() const;

  bool canStart() const;

private:
  std::array<PlayerConnection *, shared::MAX_PLAYERS_COOP> players_{};
  std::array<shared::PlayerInput, shared::MAX_PLAYERS_COOP> inputs_{};
  shared::PlayerCount players_count_{};

  shared::CoopGameSim sim_;
  shared::CoopGameState state_;
};

class PvPGame : public Game {
  struct Team {
    std::array<PlayerConnection *, shared::MAX_PLAYERS_PER_TEAM> players{};
    shared::PlayerCount players_count{};
  };
  using Teams = std::array<Team, shared::MAX_TEAMS>;

private:
  void tryStart();

public:
  PvPGame(shared::PlayerCount team_size, shared::PlayerCount team_count);

  void update(float dt) override;

  bool addPlayers(const PerSocketPlayers &players_data) override;

  void removePlayers(WsPtr ws) override;

  bool isFull() const override;

  const Teams &getTeams() const;

  void setPlayerName(shared::PlayerState &state) override;

  void setPlayersReady(WsPtr ws, bool ready) override;

  bool canStart() const;

private:
  bool allPlayersReady() const;

private:
  shared::PlayerCount team_size_{};
  shared::PlayerCount team_count_{};
  Teams teams_{};
  std::array<shared::PlayerInput,
             shared::MAX_PLAYERS_PER_TEAM * shared::MAX_TEAMS>
      inputs_{};

  shared::PvPGameSim sim_;
  shared::PvPGameState state_;
};

/// Matchmaking: assigns newly-connected players to an open game (creating
/// one if needed) and owns every `Game`'s lifetime.
class GameManager {
public:
  uint32_t next_player_id{1};
  uint32_t next_game_id{1};

public:
  CoopGame *joinOrCreateCoopGame(const PerSocketPlayers &players_data);
  CoopGame *createCoopGame();

  PvPGame *joinOrCreatePvPGame(const PerSocketPlayers &players_data,
                               shared::PlayerCount team_size,
                               shared::PlayerCount team_count);
  PvPGame *createPvPGame(shared::PlayerCount team_size,
                         shared::PlayerCount team_count);

  Game *findGameById(uint32_t id);
  void destroyGame(Game *game);
  void forEachGame(std::function<void(Game *)> fn);

private:
  std::unordered_map<uint32_t, std::unique_ptr<Game>> gamesById{};
  std::vector<CoopGame *> open_coop_games_{};
  std::unordered_map<std::pair<shared::PlayerCount, shared::PlayerCount>,
                     std::vector<PvPGame *>, shared::PairHash>
      open_pvp_games_{};
};
