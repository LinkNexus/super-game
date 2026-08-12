#include "game.h"
#include "WebSocketProtocol.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include <cstddef>
#include <optional>

void GameManager::forEachGame(std::function<void(Game *)> fn) {
  for (const auto &game : gamesById) {
    fn(game.second.get());
  }
}

Game *GameManager::joinOrCreateGame(PlayerConnection *player) {
  if (open_game_ && open_game_->getPlayerCount() < shared::MAX_PLAYERS &&
      !open_game_->isOver()) {
    open_game_->addPlayer(player);
    return open_game_;
  } else {
    auto new_game = createGame();
    new_game->addPlayer(player);
    open_game_ = new_game;
    return new_game;
  }
}

Game *GameManager::createGame() {
  auto game = std::make_unique<Game>();
  game->id = next_game_id++;
  Game *gamePtr = game.get();
  gamesById[game->id] = std::move(game);
  return gamePtr;
}

Game *GameManager::findGameById(uint32_t id) {
  auto it = gamesById.find(id);
  if (it != gamesById.end()) {
    return it->second.get();
  }
  return nullptr;
}

void GameManager::destroyGame(Game *game) {
  if (open_game_ == game)
    open_game_ = nullptr;
  gamesById.erase(game->id);
}

bool Game::isOver() const { return is_over_; }

bool Game::allPlayersReady() const {
  if (getPlayerCount() < shared::MAX_PLAYERS)
    return false;

  for (const auto &player : players_)
    if (player && !player->is_ready)
      return false;

  return true;
}

void Game::tryStart() {
  if (is_running_ || is_over_ || !allPlayersReady())
    return;

  std::array<std::optional<uint32_t>, shared::MAX_PLAYERS> playerIds;
  std::transform(players_.begin(), players_.end(), playerIds.begin(),
                 [](const auto *p) {
                   return p ? std::optional<uint32_t>(p->id) : std::nullopt;
                 });

  sim_.start(playerIds);
  is_running_ = true;
}

void Game::setPlayerReady(uint32_t playerId, bool ready) {
  for (auto player : players_) {
    if (player && player->id == playerId) {
      player->is_ready = ready;
      break;
    }
  }

  tryStart();
}

void Game::update(float dt) {
  if (!is_running_)
    return;

  std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS> inputs{};
  for (std::size_t i = 0; i < players_.size(); ++i) {
    if (!players_[i])
      continue;

    auto &p = *players_[i];
    auto buttons = p.pending_movement;
    if (p.pending_shots > 0) {
      buttons =
          static_cast<shared::Button>(buttons | shared::Button::BUTTON_SHOOT);
      p.pending_shots--;
    }
    inputs[i] = {.buttons = buttons, .player_id = p.id};
  }

  sim_.step(state_, inputs, dt);

  for (auto &p : state_.players) {
    if (!p.has_value())
      continue;

    auto it = std::find_if(players_.begin(), players_.end(),
                           [&p](const PlayerConnection *player) {
                             return player && player->id == p->id;
                           });

    if (it != players_.end()) {
      p->name = (*it)->name;
    }
  }

  nlohmann::json envelope;
  envelope["type"] = shared::ServerMessageType::GAME_STATE;
  envelope["payload"] = state_;
  auto msg = envelope.dump();

  for (const auto &p : players_)
    if (p)
      p->ws->send(msg, uWS::OpCode::TEXT);

  if (state_.phase == static_cast<uint8_t>(shared::GamePhase::GAME_OVER) ||
      state_.phase == static_cast<uint8_t>(shared::GamePhase::WON)) {
    is_running_ = false;
    is_over_ = true;
  }
}

const std::array<PlayerConnection *, shared::MAX_PLAYERS> &
Game::getPlayers() const {
  return players_;
}

std::size_t Game::getPlayerCount() const {
  std::size_t count = 0;
  for (const auto &player : players_) {
    if (player)
      ++count;
  }
  return count;
}

void Game::addPlayer(PlayerConnection *player) {
  int playerCount = getPlayerCount();

  if (playerCount >= shared::MAX_PLAYERS) {
    return; // Game is full, cannot add more players
  }

  playerCount++;

  for (size_t i = 0; i < players_.size(); ++i) {
    if (!players_[i]) {
      players_[i] = player;
      break;
    }
  }
}

void Game::removePlayer(uint32_t playerId) {
  for (size_t i = 0; i < players_.size(); ++i) {
    if (players_[i] && players_[i]->id == playerId) {
      delete players_[i];
      players_[i] = nullptr;
      return;
    }
  }
}
