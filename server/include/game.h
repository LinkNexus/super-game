#include "WebSocket.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <array>
#include <cstdint>
#include <unordered_map>

class Game;
struct PlayerConnection;

struct CoopGameType {};
struct PvPGameType {
  std::size_t team_size;
};
using GameType = std::variant<CoopGameType, PvPGameType>;

/// Per-WebSocket-connection user data (uWebSockets' `ws<PerSocketData>`
/// template parameter). Populated in the `.upgrade` handler before the
/// socket exists, then filled in further in `.open`.
struct PerSocketData {
  GameType game_type{};
  Game *game = nullptr;
  PlayerConnection *player = nullptr;
};

/// One connected player: identity, name, and the per-tick input state the
/// `.message` handler accumulates between `Game::update()` calls.
struct PlayerConnection {
  uint32_t id;
  char name[shared::MAX_NAME_LENGTH + 1]{};
  uWS::WebSocket<false, true, PerSocketData> *ws;
  shared::Button pending_movement = shared::Button::BUTTON_NONE;
  /// Shot count accumulated since the last `Game::update()` tick, so a
  /// shoot press isn't missed even if it happens between ticks.
  uint8_t pending_shots = 0;
  /// Tracks whether SHOOT was already held last message, so held-down fire
  /// only counts as one shot per press rather than one per message.
  bool prev_shoot_held = false;
  bool is_ready = false;
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
  virtual bool addPlayer(PlayerConnection *player);

  /// Frees the slot belonging to @p playerId, if present.
  virtual void removePlayer(shared::PlayerId playerId);

  bool isOver() const;

  /// Updates @p playerId's ready flag and attempts to start the match
  /// (see `tryStart()`).
  void setPlayerReady(shared::PlayerId playerId, bool ready);

  bool allPlayersReady() const;

  const std::array<PlayerConnection *, shared::MAX_PLAYERS> &getPlayers() const;

  virtual bool canStart() const = 0;

  virtual bool isFull() const = 0;

  void setPlayerName(std::optional<shared::PlayerState> &state);

  virtual void tryStart() = 0;

  std::size_t getPlayersCount() const;

protected:
  bool is_running_ = false;
  bool is_over_ = false;

  shared::GameSim sim_;
  shared::GameState state_;

  std::array<PlayerConnection *, shared::MAX_PLAYERS> players_{};
  shared::PlayerInputs inputs_{};
};

class CoopGame : public Game {
public:
  CoopGame();

  void update(float dt) override;

  bool canStart() const override;

  bool isFull() const override;

  bool isRunning() const;

private:
  void tryStart() override;
};

class PvPGame : public Game {
  using Teams =
      std::array<std::array<PlayerConnection *, shared::MAX_PLAYERS / 2>, 2>;

public:
  PvPGame(std::size_t team_size);

  void update(float dt) override;

  bool addPlayer(PlayerConnection *player) override;

  void removePlayer(uint32_t playerId) override;

  bool canStart() const override;

  bool isFull() const override;

  const Teams &getTeams() const;

private:
  std::size_t team_size_{};
  Teams teams{};

private:
  /// Starts `sim_` once `allPlayersReady()` is true; a no-op otherwise, or
  /// if the match is already running/over.
  void tryStart() override;

  std::size_t getPlayersCountInTeam(std::size_t team_index) const;
};

/// Matchmaking: assigns newly-connected players to an open game (creating
/// one if needed) and owns every `Game`'s lifetime.
class GameManager {
public:
  uint32_t next_player_id = 1;
  uint32_t next_game_id = 1;

public:
  CoopGame *joinOrCreateCoopGame(PlayerConnection *player);
  CoopGame *createCoopGame();

  PvPGame *joinOrCreatePvPGame(PlayerConnection *player, std::size_t team_size);
  PvPGame *createPvPGame(std::size_t team_size);

  Game *findGameById(uint32_t id);
  void destroyGame(Game *game);
  void forEachGame(std::function<void(Game *)> fn);

private:
  std::unordered_map<uint32_t, std::unique_ptr<Game>> gamesById{};
  CoopGame *open_coop_game_ = nullptr;
  std::unordered_map<std::size_t, PvPGame *> open_pvp_games_{};
};
