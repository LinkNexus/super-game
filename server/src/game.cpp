#include "game.h"
#include "WebSocketProtocol.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/sim/player_sim.h"
#include <cstddef>
#include <optional>

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

CoopGame *GameManager::joinOrCreateCoopGame(PlayerConnection *player) {
  if (open_coop_game_ && !open_coop_game_->isFull() &&
      !open_coop_game_->isRunning() && !open_coop_game_->isOver()) {
    open_coop_game_->addPlayer(player);
    return open_coop_game_;
  } else {
    auto new_game = createCoopGame();
    new_game->addPlayer(player);
    open_coop_game_ = new_game;
    return new_game;
  }
}

PvPGame *GameManager::createPvPGame(std::size_t team_size) {
  auto game = std::make_unique<PvPGame>(team_size);
  game->id = next_game_id++;
  PvPGame *gamePtr = game.get();
  gamesById[game->id] = std::move(game);
  return gamePtr;
}

PvPGame *GameManager::joinOrCreatePvPGame(PlayerConnection *player,
                                          std::size_t team_size) {
  auto open_game = open_pvp_games_[team_size];

  if (open_game && !open_game->isFull() && !open_game->isOver()) {
    open_game->addPlayer(player);
    return open_game;
  } else {
    auto new_game = createPvPGame(team_size);
    new_game->addPlayer(player);
    open_pvp_games_[team_size] = new_game;
    return new_game;
  }
}

Game *GameManager::findGameById(uint32_t id) {
  auto it = gamesById.find(id);
  if (it != gamesById.end()) {
    return it->second.get();
  }
  return nullptr;
}

void GameManager::destroyGame(Game *game) {
  if (open_coop_game_ == game)
    open_coop_game_ = nullptr;

  for (auto it = open_pvp_games_.begin(); it != open_pvp_games_.end(); ++it) {
    if (it->second == game) {
      open_pvp_games_.erase(it);
      break;
    }
  }

  gamesById.erase(game->id);
}

bool Game::isOver() const { return is_over_; }

void Game::setPlayerName(std::optional<shared::PlayerState> &state) {
  if (!state.has_value())
    return;

  auto it = std::find_if(players_.begin(), players_.end(),
                         [&state](const PlayerConnection *player) {
                           return player && player->id == state->id;
                         });

  if (it != players_.end()) {
    state->name = (*it)->name;
  }
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

const std::array<PlayerConnection *, shared::MAX_PLAYERS> &
Game::getPlayers() const {
  return players_;
}

bool Game::addPlayer(PlayerConnection *player) {
  if (!isFull()) {
    for (size_t i = 0; i < players_.size(); ++i) {
      if (!players_[i]) {
        players_[i] = player;
        return true;
      }
    }
  }

  return false;
}

void Game::removePlayer(shared::PlayerId playerId) {
  for (size_t i = 0; i < players_.size(); ++i) {
    if (players_[i] && players_[i]->id == playerId) {
      delete players_[i];
      players_[i] = nullptr;
      return;
    }
  }

  std::visit([playerId](auto &s) { s.removePlayer(playerId); }, sim_);
}

void CoopGame::tryStart() {
  if (is_running_ || is_over_ || !canStart())
    return;

  std::visit(shared::overloaded{
                 [this](shared::CoopGameSim &s) {
                   shared::PlayerIds playerIds{};

                   std::transform(players_.begin(), players_.end(),
                                  playerIds.begin(), [](const auto *p) {
                                    return p ? std::optional<shared::PlayerId>(
                                                   p->id)
                                             : std::nullopt;
                                  });

                   s.start(playerIds);
                 },
                 [](auto &) {}},
             sim_);

  is_running_ = true;
}

void PvPGame::tryStart() {
  if (is_running_ || is_over_ || !canStart())
    return;

  std::visit(shared::overloaded{
                 [this](shared::PvPGameSim &s) {
                   shared::PvPGameSim::PerTeamPlayerIds playerIds{};

                   std::transform(
                       teams.begin(), teams.end(), playerIds.begin(),
                       [](const auto &team) {
                         std::array<std::optional<shared::PlayerId>,
                                    shared::MAX_PLAYERS / 2>
                             teamIds{};

                         std::transform(
                             team.begin(), team.end(), teamIds.begin(),
                             [](const auto *p) {
                               return p ? std::optional<shared::PlayerId>(p->id)
                                        : std::nullopt;
                             });

                         return teamIds;
                       });

                   s.start(playerIds);
                 },
                 [](auto &) {}},
             sim_);

  is_running_ = true;
}

void Game::update(float dt) {
  if (!is_running_)
    return;

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
    inputs_[i] = {.buttons = buttons, .player_id = p.id};
  }
}

void CoopGame::update(float dt) {
  Game::update(dt);

  if (!is_running_)
    return;

  std::visit(
      shared::overloaded{
          [this, &dt](shared::CoopGameSim &sim, shared::CoopGameState &state) {
            sim.step(state, inputs_, dt);

            for (auto &p : state.players)
              setPlayerName(p);

            nlohmann::json envelope;
            envelope["type"] = shared::ServerMessageType::COOP_GAME_STATE;
            envelope["payload"] = state;
            auto msg = envelope.dump();

            for (const auto &p : players_)
              if (p)
                p->ws->send(msg, uWS::OpCode::TEXT);

            if (state.phase == static_cast<uint8_t>(
                                   shared::CoopGameSim::Phase::GAME_OVER) ||
                state.phase ==
                    static_cast<uint8_t>(shared::CoopGameSim::Phase::WON)) {
              is_running_ = false;
              is_over_ = true;
            }
          },
          [](auto &, auto &) {}},
      sim_, state_);
}

void PvPGame::update(float dt) {
  Game::update(dt);

  if (!is_running_)
    return;

  std::visit(
      shared::overloaded{
          [this, &dt](shared::PvPGameSim &sim, shared::PvPGameState &state) {
            sim.step(state, inputs_, dt);

            for (auto &t : state.teams)
              for (auto &p : t.players)
                setPlayerName(p);

            nlohmann::json envelope;
            envelope["type"] = shared::ServerMessageType::PVP_GAME_STATE;
            envelope["payload"] = state;
            auto msg = envelope.dump();

            for (const auto &p : players_)
              if (p)
                p->ws->send(msg, uWS::OpCode::TEXT);

            if (state.phase ==
                static_cast<uint8_t>(shared::PvPGameSim::Phase::END)) {
              is_running_ = false;
              is_over_ = true;
            }
          },
          [](auto &, auto &) {}},
      sim_, state_);
}

bool Game::allPlayersReady() const {
  for (const auto player : players_) {
    if (player && !player->is_ready)
      return false;
  }

  return true;
}

std::size_t Game::getPlayersCount() const {
  std::size_t count = 0;
  for (const auto &player : players_) {
    if (player)
      count++;
  }
  return count;
}

bool CoopGame::canStart() const {
  auto playerCount = getPlayersCount();

  return (playerCount >= 1 && playerCount <= shared::MAX_PLAYERS &&
          allPlayersReady());
}

CoopGame::CoopGame() {
  sim_ = shared::CoopGameSim();
  state_ = shared::CoopGameState();

  for (auto &player : players_) {
    player = nullptr;
  }
}

PvPGame::PvPGame(std::size_t team_size) {
  if (team_size == 0 || team_size > shared::MAX_PLAYERS / 2) {
    throw std::invalid_argument("Invalid team size");
  }

  team_size_ = team_size;
  sim_ = shared::PvPGameSim(team_size);
  state_ = shared::PvPGameState();

  for (auto &team : teams) {
    for (auto &player : team) {
      player = nullptr;
    }
  }
}

std::size_t PvPGame::getPlayersCountInTeam(std::size_t team_index) const {
  if (team_index >= teams.size()) {
    throw std::out_of_range("Invalid team index");
  }

  std::size_t count = 0;
  for (const auto &player : teams[team_index]) {
    if (player)
      count++;
  }
  return count;
}

bool PvPGame::canStart() const { return (isFull() && allPlayersReady()); }

bool PvPGame::addPlayer(PlayerConnection *player) {
  if (Game::addPlayer(player)) {
    for (auto &team : teams) {
      if (getPlayersCountInTeam(&team - &teams[0]) < team_size_) {
        for (auto &p : team) {
          if (!p) {
            p = player;
            return true;
          }
        }
      }
    }
  }

  return false;
}

void PvPGame::removePlayer(shared::PlayerId playerId) {
  for (auto &team : teams) {
    for (auto &p : team) {
      if (p && p->id == playerId) {
        p = nullptr;
        Game::removePlayer(playerId);
        return;
      }
    }
  }
}

bool CoopGame::isFull() const {
  return getPlayersCount() >= shared::MAX_PLAYERS;
}

bool PvPGame::isFull() const {
  for (const auto &team : teams) {
    if (getPlayersCountInTeam(&team - &teams[0]) < team_size_) {
      return false;
    }
  }

  return true;
}

const PvPGame::Teams &PvPGame::getTeams() const { return teams; }

bool CoopGame::isRunning() const { return is_running_; }
