#include "WebSocket.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <array>
#include <cstdint>
#include <unordered_map>

class Game;
struct PlayerConnection;

/// Per-WebSocket-connection user data (uWebSockets' `ws<PerSocketData>`
/// template parameter). Populated in the `.upgrade` handler before the
/// socket exists, then filled in further in `.open`.
struct PerSocketData {
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

/// One authoritative match: up to `MAX_PLAYERS` connections plus the
/// `GameSim` they drive. Owned by `GameManager`; runs its own `GameSim`
/// independently of any other concurrent match.
class Game {
public:
  uint32_t id;

public:
  /// Steps the simulation by @p dt using each player's accumulated
  /// pending input, then broadcasts the resulting `GameState` to every
  /// connected player. No-op until `tryStart()` has actually started the
  /// match.
  void update(float dt);

  /// Assigns @p player to the first free slot, if the match isn't already
  /// full.
  void addPlayer(PlayerConnection *player);

  /// Frees the slot belonging to @p playerId, if present.
  void removePlayer(uint32_t playerId);

  std::size_t getPlayerCount() const;
  const std::array<PlayerConnection *, shared::MAX_PLAYERS> &getPlayers() const;

  /// @return true once the match has reached `GamePhase::WON` or
  /// `GamePhase::GAME_OVER` - `GameManager` destroys the game shortly
  /// after this becomes true.
  bool isOver() const;

  /// Updates @p playerId's ready flag and attempts to start the match
  /// (see `tryStart()`).
  void setPlayerReady(uint32_t playerId, bool ready);

  /// @return true once every player slot is filled and every connected
  /// player is ready.
  bool allPlayersReady() const;

private:
  bool is_running_ = false;
  bool is_over_ = false;
  shared::GameSim sim_;
  shared::GameState state_;
  std::array<PlayerConnection *, shared::MAX_PLAYERS> players_{};

private:
  /// Starts `sim_` once `allPlayersReady()` is true; a no-op otherwise, or
  /// if the match is already running/over.
  void tryStart();
};

/// Matchmaking: assigns newly-connected players to an open game (creating
/// one if needed) and owns every `Game`'s lifetime.
class GameManager {
public:
  uint32_t next_player_id = 1;
  uint32_t next_game_id = 1;

public:
  /// Adds @p player to the currently-open game, or creates a new one if
  /// there isn't one with a free slot. A third/later connection while one
  /// game is already full starts an independent second match rather than
  /// being rejected.
  Game *joinOrCreateGame(PlayerConnection *player);

  Game *createGame();
  Game *findGameById(uint32_t id);
  void destroyGame(Game *game);
  void forEachGame(std::function<void(Game *)> fn);

private:
  std::unordered_map<uint32_t, std::unique_ptr<Game>> gamesById{};
  /// The game currently accepting new players, if any.
  Game *open_game_ = nullptr;
};
