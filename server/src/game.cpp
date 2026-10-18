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

PvPGame::PvPGame(shared::PlayerCount team_size,
                 shared::PlayerCount team_count) {
  team_size_ = team_size;
  team_count_ = team_count;

  for (std::size_t teamIdx = 0; teamIdx < team_count_;) {
    auto &t = teams_[teamIdx].emplace();
    t.id = ++teamIdx;
  }
}

bool Game::isOver() const { return is_over_; }

static inline void
initPlayerConnection(PlayerConnection &p,
                     const std::pair<ServerPlayerId, std::string> &player,
                     WsConnection *ws) {
  p.id = player.first;
  p.ws = ws;
  p.name = player.second;
}

bool CoopGame::addPlayers(const PerSocketPlayers &players_data) {
  if (!is_running_ && !is_over_ &&
      shared::count_optional(players_) +
              shared::count_optional(players_data.players) <=
          shared::MAX_PLAYERS_COOP) {
    std::size_t slot = 0;
    for (const auto &p : players_data.players) {
      if (p) {
        while (players_[slot])
          ++slot;

        initPlayerConnection(players_[slot++].emplace(), *p, players_data.ws);
      }
    }

    player_sockets_.push_back(players_data.ws);

    tryStart();
    return true;
  }

  return false;
}

bool PvPGame::addPlayers(const PerSocketPlayers &players_data) {
  if (!is_running_ && !is_over_) {
    for (auto &team : teams_) {
      if (team && shared::count_optional(team->players) +
                          shared::count_optional(players_data.players) <=
                      team_size_) {
        std::size_t slot{0};

        for (const auto &p : players_data.players) {
          if (p) {
            while (team->players[slot])
              ++slot;

            initPlayerConnection(team->players[slot++].emplace(), *p,
                                 players_data.ws);
          }
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
      sim_.removePlayer(player->id);
      players_[i] = std::nullopt;
    }
  }

  player_sockets_.erase(
      std::remove(player_sockets_.begin(), player_sockets_.end(), ws),
      player_sockets_.end());
}

void PvPGame::removePlayers(const WsConnection *ws) {
  for (auto &team : teams_) {
    if (team) {
      for (auto &player : team->players) {
        if (player && player->ws == ws) {
          sim_.removePlayer(player->id);
          player = std::nullopt;
        }
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

  sim_ = shared::CoopGameSim();
  state_ = shared::CoopGameState();

  shared::PlayerIds<shared::MAX_PLAYERS_COOP> ids{};
  std::transform(players_.begin(), players_.end(), ids.begin(),
                 [](const auto &player) {
                   return player ? std::optional(player->id) : std::nullopt;
                 });

  sim_.start(ids);

  is_running_ = true;
}

void PvPGame::tryStart() {
  if (is_running_ || is_over_ || !canStart())
    return;

  sim_ = shared::PvPGameSim();
  state_ = shared::PvPGameState();

  shared::PvPGameSim::TeamRosters ids{};
  auto teamIdx{0ul};
  for (const auto &team : teams_) {
    if (team) {
      auto &[rId, rPlayers] = ids[teamIdx++].emplace();
      rId = team->id;

      auto playerIdx{0ul};
      for (const auto &player : team->players) {
        if (player) {
          rPlayers[playerIdx++] = player->id;
        }
      }
    }
  }

  sim_.start(ids);

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
                    .player_id = player->id};
    }
  }

  sim_.step(state_, inputs_, dt);

  for (auto &player : players_) {
    auto stateIt = std::find_if(state_.players.begin(), state_.players.end(),
                                [&player](const auto &s) {
                                  return s && player && s->id == player->id;
                                });

    if (stateIt != state_.players.end()) {
      (*stateIt)->name = player->name;
    }
  }

  broadcast(state_);

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

  auto inputIdx{0ul};
  for (auto &team : teams_) {
    if (team) {
      for (auto &player : team->players) {
        if (player) {
          inputs_[inputIdx++] = {.buttons = getPlayerInput(player.value()),
                                 .player_id = player->id};
        }
      }
    }
  }

  sim_.step(state_, inputs_, dt);

  for (auto &team : teams_) {
    if (!team)
      continue;

    auto teamStateIt =
        std::find_if(state_.teams.begin(), state_.teams.end(),
                     [&team](const auto &s) { return s && s->id == team->id; });

    for (auto &player : team->players) {
      if (teamStateIt == state_.teams.end())
        continue;

      auto playerStateIt =
          std::find_if((*teamStateIt)->players.begin(),
                       (*teamStateIt)->players.end(), [&player](const auto &s) {
                         return s && player && s->id == player->id;
                       });

      if (playerStateIt != (*teamStateIt)->players.end()) {
        (*playerStateIt)->name = player->name;
      }
    }
  }

  broadcast(state_);

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
  for (const auto &team : teams_) {
    if (!team)
      continue;

    for (const auto &player : team->players) {
      if (player && !player->is_ready)
        return false;
    }
  }

  return true;
}

bool Game::canStart() const {
  return (thereIsEnoughPlayers() && allPlayersReady());
}

bool CoopGame::thereIsEnoughPlayers() const {
  return player_sockets_.size() > 1;
}

bool PvPGame::thereIsEnoughPlayers() const {
  for (const auto &team : teams_) {
    if (!team)
      continue;

    if (shared::count_optional(team->players) < team_size_) {
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
  for (auto &team : teams_) {
    if (!team)
      continue;

    for (auto &player : team->players) {
      if (player && player->ws == ws) {
        player->is_ready = ready;
      }
    }
  }

  tryStart();
}

bool Game::isRunning() const { return is_running_; }

const CoopGame::Players &CoopGame::getPlayers() const { return players_; }

bool Game::isEmpty() const { return player_sockets_.empty(); }

PlayerConnection *PvPGame::findPlayerById(ServerPlayerId id) {
  for (auto &team : teams_) {
    auto it = std::find_if(
        team->players.begin(), team->players.end(),
        [id](const auto &player) { return player && player->id == id; });

    if (it != team->players.end()) {
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

shared::LobbyUpdate PvPGame::buildLobbyUpdate() const {
  shared::PvPGameLobbyUpdate::TeamInfos teamsInfos{};

  for (auto i{0ul}; i < team_count_; ++i) {
    teamsInfos[i].emplace();
  }

  auto slot{0ul};
  for (const auto &team : teams_) {
    if (!team)
      continue;

    auto &tInfo = teamsInfos[slot++];

    if (!tInfo)
      continue;

    for (auto i{0ul}; i < team->players.size(); ++i) {
      const auto &player = team->players[i];

      if (player)
        (*tInfo)[i] = shared::PlayerInfo{.id = player->id,
                                         .name = player->name,
                                         .is_ready = player->is_ready};
    }
  }

  return shared::PvPGameLobbyUpdate{.teams = teamsInfos,
                                    .team_size = team_size_,
                                    .game_started = is_running_};
}

shared::LobbyUpdate CoopGame::buildLobbyUpdate() const {
  shared::CoopGameLobbyUpdate::PlayerInfos playersInfos{};

  for (auto i{0ul}; i < players_.size(); ++i) {
    const auto &player = players_[i];

    if (player) {
      playersInfos[i] = shared::PlayerInfo{
          .id = player->id, .name = player->name, .is_ready = player->is_ready};
    }
  }

  return shared::CoopGameLobbyUpdate{.players = playersInfos,
                                     .game_started = is_running_};
}

void Game::broadcastLobbyUpdate() const {
  std::visit([this](const auto &u) { broadcast(u); }, buildLobbyUpdate());
}
