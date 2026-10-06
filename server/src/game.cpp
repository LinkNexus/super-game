#include "game.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "types.h"
#include <cstddef>

const std::vector<WsConnection *> &Game::getPlayerSockets() const {
  return player_sockets_;
}

bool Game::isOver() const { return is_over_; }

inline void initPlayerConnection(PlayerConnection &p, ServerPlayerId id,
                                 WsConnection *ws, const std::string &name) {
  p.id = id;
  p.ws = ws;
  p.name = name;
}

bool CoopGame::addPlayers(const PerSocketPlayers &players_data) {
  auto players_count = shared::count_optional(players_);

  if (!is_running_ && !is_over_ &&
      players_count + players_data.count <= shared::MAX_PLAYERS_COOP) {

    std::size_t slot = 0;
    for (std::size_t i = 0; i < players_data.count; ++i) {
      while (players_[slot])
        ++slot;

      auto &p = players_[slot++].emplace();
      initPlayerConnection(p, players_data.players[i], players_data.ws,
                           players_data.names[i]);
    }

    player_sockets_.push_back(players_data.ws);

    tryStart();
    return true;
  }

  return false;
}

bool PvPGame::addPlayers(const PerSocketPlayers &players_data) {
  if (!is_running_ && !is_over_) {
    for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
      auto &team = teams_[teamIdx];
      auto teamPlayerCount = shared::count_optional(team.players);

      if (teamPlayerCount + players_data.count <= team_size_) {
        std::size_t slot = 0;

        for (std::size_t i = 0; i < players_data.count; ++i) {
          while (team.players[slot])
            ++slot;

          auto &p = team.players[slot++].emplace();
          initPlayerConnection(p, players_data.players[i], players_data.ws,
                               players_data.names[i]);
        }

        player_sockets_.push_back(players_data.ws);

        tryStart();
        return true;
      }
    }
  }

  return false;
}

void CoopGame::removePlayers(const WsConnection *ws) {
  for (std::size_t i = 0; i < players_.size(); ++i) {
    const auto &player = players_[i];

    if (player && player->ws == ws) {
      sim_.removePlayer(player->id_in_game);
      players_[i] = std::nullopt;
    }
  }

  player_sockets_.erase(
      std::remove(player_sockets_.begin(), player_sockets_.end(), ws),
      player_sockets_.end());
}

void PvPGame::removePlayers(const WsConnection *ws) {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];

    for (auto &player : team.players) {
      if (player && player->ws == ws) {
        sim_.removePlayer(player->id_in_game);
        player = std::nullopt;
      }
    }
  }

  player_sockets_.erase(
      std::remove(player_sockets_.begin(), player_sockets_.end(), ws),
      player_sockets_.end());
}

void CoopGame::tryStart() {
  if (is_running_ || is_over_ || !canStart())
    return;

  auto playersCount = shared::count_optional(players_);
  sim_ = shared::CoopGameSim();
  sim_.init(playersCount);
  state_ = shared::CoopGameState();
  auto ids = sim_.start();

  shared::PlayerCount idIdx{};
  for (auto &player : players_) {
    if (player) {
      player->id_in_game = ids[idIdx++];
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

    shared::PlayerCount idIdx{};
    for (auto &player : team.players) {
      if (player) {
        player->id_in_game = ids[teamIdx][idIdx++];
      }
    }
  }

  is_running_ = true;
}

inline shared::Button getPlayerInput(PlayerConnection &player) {
  auto buttons = player.pending_movement;
  if (player.pending_shots > 0) {
    buttons =
        static_cast<shared::Button>(buttons | shared::Button::BUTTON_SHOOT);
    player.pending_shots--;
  }

  return buttons;
}

void CoopGame::update(float dt) {
  if (!is_running_)
    return;

  inputs_.fill(std::nullopt);
  for (std::size_t i = 0; i < players_.size(); ++i) {
    if (auto &player = players_[i]) {
      inputs_[i] = {.buttons = getPlayerInput(player.value()),
                    .player_id = player->id_in_game};
    }
  }

  std::array<shared::PlayerInput, shared::MAX_PLAYERS_COOP> simInputs{};
  shared::PlayerCount idx{};
  for (const auto &input : inputs_) {
    if (input) {
      simInputs[idx++] = input.value();
    }
  }

  sim_.step(state_, simInputs, dt);

  for (auto &player : players_) {
    auto stateIt = std::find_if(state_.players.begin(), state_.players.end(),
                                [&player](const auto &s) {
                                  return player && s.id == player->id_in_game;
                                });

    if (stateIt != state_.players.end()) {
      stateIt->name = player->name;
    }
  }

  nlohmann::json envelope;
  envelope["type"] = shared::ServerMessageType::COOP_GAME_STATE;
  envelope["payload"] = state_;

  for (const auto &ws : player_sockets_) {
    ws->send(envelope.dump());
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

  inputs_.fill(std::nullopt);
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];
    for (std::size_t playerIdx = 0; playerIdx < team.players.size();
         ++playerIdx) {
      if (auto &player = team.players[playerIdx]) {
        inputs_[shared::MAX_PLAYERS_PER_TEAM * teamIdx + playerIdx] = {
            .buttons = getPlayerInput(player.value()),
            .player_id = player->id_in_game};
      }
    }
  }

  std::array<shared::PlayerInput,
             shared::MAX_PLAYERS_PER_TEAM * shared::MAX_TEAMS>
      simInputs{};

  shared::PlayerCount idx{};
  for (const auto &input : inputs_) {
    if (input) {
      simInputs[idx++] = input.value();
    }
  }

  sim_.step(state_, simInputs, dt);

  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    for (auto &player : teams_[teamIdx].players) {
      auto stateIt = std::find_if(state_.teams[teamIdx].players.begin(),
                                  state_.teams[teamIdx].players.end(),
                                  [&player](const auto &s) {
                                    return player && s.id == player->id_in_game;
                                  });

      if (stateIt != state_.teams[teamIdx].players.end()) {
        stateIt->name = player->name;
      }
    }
  }

  nlohmann::json envelope;
  envelope["type"] = shared::ServerMessageType::PVP_GAME_STATE;
  envelope["payload"] = state_;

  for (const auto &ws : player_sockets_) {
    ws->send(envelope.dump());
  }

  if (state_.phase == static_cast<uint8_t>(shared::PvPGameSim::Phase::END)) {
    is_running_ = false;
    is_over_ = true;
  }
}

bool CoopGame::allPlayersReady() const {
  return std::all_of(players_.begin(), players_.end(), [](const auto &player) {
    return !player || player->is_ready;
  });
}

bool PvPGame::allPlayersReady() const {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    const auto &team = teams_[teamIdx];

    for (const auto &player : team.players) {
      if (player && !player->is_ready)
        return false;
    }
  }

  return true;
}

bool Game::canStart() const {
  return (thereIsEnoughPlayers() && allPlayersReady());
}

PvPGame::PvPGame(shared::PlayerCount team_size,
                 shared::PlayerCount team_count) {
  team_size_ = team_size;
  team_count_ = team_count;
}

bool CoopGame::thereIsEnoughPlayers() const {
  return player_sockets_.size() > 1;
}

bool PvPGame::thereIsEnoughPlayers() const {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    if (shared::count_optional(teams_[teamIdx].players) < team_size_) {
      return false;
    }
  }

  return true;
}

const PvPGame::Teams &PvPGame::getTeams() const { return teams_; }

void CoopGame::setPlayersReady(const WsConnection *ws, bool ready) {
  for (auto &player : players_) {
    if (player && player->ws == ws) {
      player->is_ready = ready;
    }
  }

  tryStart();
}

void PvPGame::setPlayersReady(const WsConnection *ws, bool ready) {
  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    auto &team = teams_[teamIdx];

    for (auto &player : team.players) {
      if (player && player->ws == ws) {
        player->is_ready = ready;
      }
    }
  }

  tryStart();
}

bool Game::isRunning() const { return is_running_; }

const std::array<std::optional<PlayerConnection>, shared::MAX_PLAYERS_COOP> &
CoopGame::getPlayers() const {
  return players_;
}

bool Game::isEmpty() const { return player_sockets_.empty(); }

PlayerConnection *PvPGame::findPlayerById(ServerPlayerId id) {
  for (auto &team : teams_) {
    auto it = std::find_if(
        team.players.begin(), team.players.end(),
        [id](const auto &player) { return player && player->id == id; });

    if (it != team.players.end()) {
      return &(it->value());
    }
  }

  return nullptr;
}

PlayerConnection *CoopGame::findPlayerById(ServerPlayerId id) {
  auto it =
      std::find_if(players_.begin(), players_.end(), [id](const auto &player) {
        return player && player->id == id;
      });

  if (it != players_.end()) {
    return &(it->value());
  }

  return nullptr;
}
