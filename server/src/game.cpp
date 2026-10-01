#include "game.h"
#include "WebSocketProtocol.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <cstddef>

void GameManager::forEachGame(std::function<void(Game *)> fn) {
  for (const auto &game : gamesById) {
    fn(game.second.get());
  }
}

CoopGame *GameManager::createCoopGame() {
  auto game = std::make_unique<CoopGame>();
  game->id = next_game_id++;
  CoopGame *gamePtr = game.get();
  gamesById[game->id] = std::move(game);
  return gamePtr;
}

CoopGame *
GameManager::joinOrCreateCoopGame(const PerSocketPlayers &players_data) {
  for (auto game : open_coop_games_) {
    if (game && !game->isFull() && !game->isRunning() && !game->isOver() &&
        game->addPlayers(players_data)) {
      return game;
    }
  }

  auto new_game = createCoopGame();
  new_game->addPlayers(players_data);
  open_coop_games_.push_back(new_game);
  return new_game;
}

PvPGame *GameManager::createPvPGame(shared::PlayerCount team_size,
                                    shared::PlayerCount team_count) {
  auto game = std::make_unique<PvPGame>(team_size, team_count);
  game->id = next_game_id++;
  PvPGame *gamePtr = game.get();
  gamesById[game->id] = std::move(game);
  return gamePtr;
}

PvPGame *GameManager::joinOrCreatePvPGame(const PerSocketPlayers &players_data,
                                          shared::PlayerCount team_size,
                                          shared::PlayerCount team_count) {
  auto &openGames = open_pvp_games_[{team_count, team_size}];

  for (auto game : openGames) {
    if (game && !game->isFull() && !game->isRunning() && !game->isOver() &&
        game->addPlayers(players_data)) {
      return game;
    }
  }

  auto new_game = createPvPGame(team_size, team_count);
  new_game->addPlayers(players_data);
  openGames.push_back(new_game);
  return new_game;
}

Game *GameManager::findGameById(uint32_t id) {
  auto it = gamesById.find(id);
  if (it != gamesById.end()) {
    return it->second.get();
  }
  return nullptr;
}

void GameManager::destroyGame(Game *game) {
  open_coop_games_.erase(
      std::remove(open_coop_games_.begin(), open_coop_games_.end(), game),
      open_coop_games_.end());

  for (auto &kvp : open_pvp_games_) {
    auto it = std::find(kvp.second.begin(), kvp.second.end(), game);

    if (it != kvp.second.end()) {
      kvp.second.erase(std::remove(kvp.second.begin(), kvp.second.end(), game),
                       kvp.second.end());
      break;
    }
  }

  gamesById.erase(game->id);
}

const std::vector<WsPtr> &Game::getPlayerSockets() const {
  return player_sockets_;
}

bool Game::isOver() const { return is_over_; }

bool CoopGame::addPlayers(const PerSocketPlayers &players_data) {
  if (!is_running_ && !isFull() &&
      players_count_ + players_data.count <= shared::MAX_PLAYERS_COOP) {
    for (std::size_t i = 0; i < players_data.count; ++i) {
      players_[players_count_ + i] = players_data.players[i];
    }

    players_count_ += players_data.count;
    player_sockets_.push_back(players_data.ws);

    tryStart();
    return true;
  }

  return false;
}

bool PvPGame::addPlayers(const PerSocketPlayers &players_data) {
  if (!is_running_ && !isFull()) {
    for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
      auto &team = teams_[teamIdx];

      if (team.players_count + players_data.count <= team_size_) {
        for (std::size_t i = 0; i < players_data.count; ++i) {
          team.players[team.players_count + i] = players_data.players[i];
        }

        team.players_count += players_data.count;
        player_sockets_.push_back(players_data.ws);

        tryStart();
        return true;
      }
    }
  }

  return false;
}

void CoopGame::removePlayers(WsPtr ws) {
  auto playerCount = players_count_;

  for (std::size_t i = 0; i < playerCount; ++i) {
    const auto player = players_[i];

    if (player && player->ws == ws) {
      sim_.removePlayer(player->id_in_game);
      players_[i] = nullptr;
      player_sockets_.erase(
          std::remove(player_sockets_.begin(), player_sockets_.end(), ws),
          player_sockets_.end());
      players_[i] = std::move(players_[--players_count_]);
    }
  }
}

void PvPGame::removePlayers(WsPtr ws) {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];

    auto teamPlayerCount = team.players_count;
    for (std::size_t playerIdx = 0; playerIdx < teamPlayerCount; ++playerIdx) {
      const auto &player = team.players[playerIdx];

      if (player && player->ws == ws) {
        sim_.removePlayer(player->id_in_game);
        player_sockets_.erase(
            std::remove(player_sockets_.begin(), player_sockets_.end(), ws),
            player_sockets_.end());
        team.players[playerIdx] = std::move(team.players[--team.players_count]);
      }
    }
  }
}

void CoopGame::tryStart() {
  if (is_running_ || is_over_ || !canStart())
    return;

  sim_ = shared::CoopGameSim();
  sim_.init(players_count_);
  state_ = shared::CoopGameState();
  auto ids = sim_.start();

  for (std::size_t i = 0; i < players_count_; ++i) {
    if (players_[i]) {
      players_[i]->id_in_game = ids[i];
    }
  }

  is_running_ = true;
}

void PvPGame::tryStart() {
  if (is_running_ || is_over_ || !canStart())
    return;

  sim_ = shared::PvPGameSim();
  sim_.init(team_size_, team_count_);
  state_ = shared::PvPGameState();
  auto ids = sim_.start();

  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];
    for (std::size_t playerIdx = 0; playerIdx < team.players_count;
         ++playerIdx) {
      team.players[playerIdx]->id_in_game = ids[teamIdx][playerIdx];
    }
  }

  is_running_ = true;
}

shared::Button getPlayerInput(PlayerConnection *player) {
  auto buttons = player->pending_movement;
  if (player->pending_shots > 0) {
    buttons =
        static_cast<shared::Button>(buttons | shared::Button::BUTTON_SHOOT);
    player->pending_shots--;
  }

  return buttons;
}

void CoopGame::update(float dt) {
  if (!is_running_)
    return;

  for (std::size_t i = 0; i < players_count_; ++i) {
    inputs_[i] = {.buttons = getPlayerInput(players_[i]),
                  .player_id = players_[i]->id_in_game};
  }

  sim_.step(state_, inputs_, dt);

  for (std::size_t i = 0; i < players_count_; ++i) {
    setPlayerName(state_.players[i]);
  }

  nlohmann::json envelope;
  envelope["type"] = shared::ServerMessageType::COOP_GAME_STATE;
  envelope["payload"] = state_;

  for (const auto &ws : player_sockets_) {
    ws->send(envelope.dump(), uWS::OpCode::TEXT);
  }

  if (state_.phase ==
          static_cast<uint8_t>(shared::CoopGameSim::Phase::GAME_OVER) ||
      state_.phase == static_cast<uint8_t>(shared::CoopGameSim::Phase::WON)) {
    is_running_ = false;
    is_over_ = true;
  }
}

void PvPGame::update(float dt) {
  if (!is_running_)
    return;

  std::size_t idx{};
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];
    for (std::size_t playerIdx = 0; playerIdx < team.players_count;
         ++playerIdx) {
      auto &player = team.players[playerIdx];
      inputs_[idx++] = {.buttons = getPlayerInput(player),
                        .player_id = player->id_in_game};
    }
  }

  sim_.step(state_, inputs_, dt);

  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    for (std::size_t playerIdx = 0; playerIdx < teams_[teamIdx].players_count;
         ++playerIdx) {
      setPlayerName(state_.teams[teamIdx].players[playerIdx]);
    }
  }

  nlohmann::json envelope;
  envelope["type"] = shared::ServerMessageType::PVP_GAME_STATE;
  envelope["payload"] = state_;

  for (const auto &ws : player_sockets_) {
    ws->send(envelope.dump(), uWS::OpCode::TEXT);
  }

  if (state_.phase == static_cast<uint8_t>(shared::PvPGameSim::Phase::END)) {
    is_running_ = false;
    is_over_ = true;
  }
}

bool CoopGame::allPlayersReady() const {
  for (std::size_t i = 0; i < players_count_; ++i) {
    if (players_[i] && !players_[i]->is_ready)
      return false;
  }

  return true;
}

bool PvPGame::allPlayersReady() const {
  for (const auto &team : teams_) {
    for (std::size_t i = 0; i < team.players_count; ++i) {
      if (team.players[i] && !team.players[i]->is_ready)
        return false;
    }
  }

  return true;
}

bool CoopGame::canStart() const { return (isFull() && allPlayersReady()); }

bool PvPGame::canStart() const { return (isFull() && allPlayersReady()); }

PvPGame::PvPGame(shared::PlayerCount team_size,
                 shared::PlayerCount team_count) {
  team_size_ = team_size;
  team_count_ = team_count;
}

bool CoopGame::isFull() const {
  return player_sockets_.size() > 1 &&
         players_count_ <= shared::MAX_PLAYERS_COOP;
}

bool PvPGame::isFull() const {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    if (teams_[teamIdx].players_count < team_size_) {
      return false;
    }
  }

  return true;
}

const PvPGame::Teams &PvPGame::getTeams() const { return teams_; }

void CoopGame::setPlayerName(shared::PlayerState &state) {
  for (std::size_t i = 0; i < players_count_; ++i) {
    if (players_[i] && players_[i]->id == state.id) {
      state.name = players_[i]->name;
      return;
    }
  }
}

void CoopGame::setPlayersReady(WsPtr ws, bool ready) {
  for (std::size_t i = 0; i < players_count_; ++i) {
    if (players_[i] && players_[i]->ws == ws) {
      players_[i]->is_ready = ready;
    }
  }

  tryStart();
}

shared::PlayerCount CoopGame::getPlayersCount() const { return players_count_; }

void PvPGame::setPlayerName(shared::PlayerState &state) {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];
    for (std::size_t playerIdx = 0; playerIdx < team.players_count;
         ++playerIdx) {
      auto &player = team.players[playerIdx];
      if (player && player->id == state.id) {
        state.name = player->name;
        return;
      }
    }
  }
}

void PvPGame::setPlayersReady(WsPtr ws, bool ready) {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];
    for (std::size_t playerIdx = 0; playerIdx < team.players_count;
         ++playerIdx) {
      auto &player = team.players[playerIdx];
      if (player && player->ws == ws) {
        player->is_ready = ready;
      }
    }
  }

  tryStart();
}

bool Game::isRunning() const { return is_running_; }

const std::array<PlayerConnection *, shared::MAX_PLAYERS_COOP> &
CoopGame::getPlayers() const {
  return players_;
}
