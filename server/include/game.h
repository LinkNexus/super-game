#include "WebSocket.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <array>
#include <cstdint>
#include <unordered_map>

class Game;
struct PlayerConnection;

struct PerSocketData {
  Game *game = nullptr;
  PlayerConnection *player = nullptr;
};

struct PlayerConnection {
  uint32_t id;
  uWS::WebSocket<false, true, PerSocketData> *ws;
  shared::Button pending_movement = shared::Button::BUTTON_NONE;
  uint8_t pending_shots = 0;
  bool prev_shoot_held = false;
};

class Game {
public:
  uint32_t id;

public:
  void update(float dt);
  void addPlayer(PlayerConnection *player);
  void removePlayer(uint32_t playerId);
  std::size_t getPlayerCount() const;
  const std::array<PlayerConnection *, shared::MAX_PLAYERS> &getPlayers() const;

private:
  bool is_running_ = false;
  shared::GameSim sim_;
  shared::GameState state_;
  std::array<PlayerConnection *, shared::MAX_PLAYERS> players_;
};

class GameManager {
public:
  uint32_t next_player_id = 1;
  uint32_t next_game_id = 1;

public:
  Game *joinOrCreateGame(PlayerConnection *player);
  Game *createGame();
  Game *findGameById(uint32_t id);
  void destroyGame(Game *game);
  void forEachGame(std::function<void(Game *)> fn);

private:
  std::unordered_map<uint32_t, std::unique_ptr<Game>> gamesById;
  Game *open_game_ = nullptr;
};
