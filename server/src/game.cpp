#include "game.h"
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

  for (std::size_t teamIdx = 0; teamIdx < team_count_; ++teamIdx) {
    team_ids_[teamIdx] = shared::TeamId(teamIdx + 1);
  }
}

bool Game::isOver() const { return is_over_; }

bool CoopGame::addPlayers(const PerSocketPlayers &players_data) {
  if (!is_running_ && !is_over_ &&
      shared::count_optional(players_) +
              shared::count_optional(players_data.players) <=
          shared::MAX_PLAYERS_COOP) {
    for (const auto &player : players_data.players) {
      if (!player)
        continue;
      insertPlayer(player.value(), players_data.ws);
    }

    player_sockets_.push_back(players_data.ws);
    tryStart();
    return true;
  }

  return false;
}

bool PvPGame::addPlayers(const PerSocketPlayers &players_data) {
  if (!is_running_ && !is_over_) {
    std::size_t availableSlots{0};
    const auto neededSlots = shared::count_optional(players_data.players);
    std::optional<std::size_t> together;

    const auto freeSlots = [this](std::size_t teamIdx) {
      return team_size_ - countPlayersOfTeam(*team_ids_[teamIdx]);
    };

    for (std::size_t i = 0; i < team_count_; ++i) {
      if (!team_ids_[i])
        continue;

      auto free = freeSlots(i);
      availableSlots += free;

      if (!together && free >= neededSlots)
        together = i;
    }

    if (availableSlots < neededSlots)
      return false;

    const auto firstTeamWithAvailableSlot = [this]() {
      for (std::size_t i = 0; i < team_ids_.size(); ++i) {
        const auto &teamId = team_ids_[i];

        if (!teamId)
          continue;

        auto playersCount = countPlayersOfTeam(teamId.value());

        if (playersCount < team_size_)
          return std::optional{i};
      }

      return std::optional<std::size_t>{std::nullopt};
    };

    for (const auto &player : players_data.players) {
      if (!player)
        continue;

      auto teamIdx = together ? *together : *firstTeamWithAvailableSlot();
      insertPlayer(*player, players_data.ws,
                   std::pair{teamIdx, *team_ids_[teamIdx]});
    }

    player_sockets_.push_back(players_data.ws);
    tryStart();
    return true;
  }

  return false;
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

  std::get<shared::CoopGameSim>(sim_).start(ids);

  is_running_ = true;
}

void PvPGame::tryStart() {
  if (is_running_ || is_over_ || !canStart())
    return;

  sim_ = shared::PvPGameSim();
  state_ = shared::PvPGameState();

  shared::PvPGameSim::TeamRosters ids{};
  shared::iterateOverOptionals(
      team_ids_, [this, &ids](const auto &id, std::size_t idx) {
        auto &[rId, rPlayers] = ids[idx].emplace();
        rId = id;

        forEachPlayerOfTeam(id, [&](const auto &player, std::size_t rank) {
          rPlayers[rank] = player.id;
        });
      });

  std::get<shared::PvPGameSim>(sim_).start(ids);

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
  shared::iterateOverOptionals(players_, [this](PlayerConnection &player,
                                                std::size_t idx) {
    inputs_[idx] = {.buttons = getPlayerInput(player), .player_id = player.id};
  });

  std::visit(
      shared::overloaded{
          [this, &dt](shared::CoopGameSim &sim, shared::CoopGameState &state) {
            sim.step(state, inputs_, dt);

            shared::iterateOverOptionals(players_, [&state](auto &player,
                                                            std::size_t) {
              auto stateIt = std::find_if(
                  state.players.begin(), state.players.end(),
                  [&player](const auto &s) { return s && s->id == player.id; });

              if (stateIt != state.players.end()) {
                (*stateIt)->name = player.name;
              }
            });

            if (state.phase == shared::CoopGamePhase::GAME_OVER ||
                state.phase == shared::CoopGamePhase::WON) {
              is_running_ = false;
              is_over_ = true;
            }

            broadcast(state);
          },
          [](auto &, auto &) {}},
      sim_, state_);
}

void PvPGame::update(float dt) {
  if (!is_running_)
    return;

  // No team grouping needed: the slot already is teamIdx * PER_TEAM + rank.
  inputs_.fill(std::nullopt);
  shared::iterateOverOptionals(players_, [this](PlayerConnection &player,
                                                auto slot) {
    inputs_[slot] = {.buttons = getPlayerInput(player), .player_id = player.id};
  });

  std::visit(
      shared::overloaded{
          [&](shared::PvPGameSim &sim, shared::PvPGameState &state) {
            sim.step(state, inputs_, dt);

            shared::iterateOverOptionals(team_ids_, [&](const auto &teamId,
                                                        std::size_t) {
              auto teamStateIt = std::find_if(
                  state.teams.begin(), state.teams.end(),
                  [&teamId](const auto &s) { return s && s->id == teamId; });

              if (teamStateIt == state.teams.end())
                return;

              shared::iterateOverOptionals(players_, [&](auto &player,
                                                         std::size_t) {
                auto playerStateIt = std::find_if(
                    (*teamStateIt)->players.begin(),
                    (*teamStateIt)->players.end(), [&player](const auto &s) {
                      return s && s->id == player.id;
                    });

                if (playerStateIt != (*teamStateIt)->players.end()) {
                  (*playerStateIt)->name = player.name;
                }
              });
            });

            if (state.phase == shared::PvPGamePhase::END) {
              is_running_ = false;
              is_over_ = true;
            }

            broadcast(state);
          },
          [](auto &, auto &) {}},
      sim_, state_);
}

bool Game::canStart() const {
  return (thereIsEnoughPlayers() && allPlayersReady());
}

bool CoopGame::thereIsEnoughPlayers() const {
  return player_sockets_.size() > 1;
}

bool PvPGame::thereIsEnoughPlayers() const {
  for (const auto &teamId : team_ids_) {
    if (!teamId)
      continue;

    if (std::count_if(players_.begin(), players_.end(),
                      [&teamId](const auto &player) {
                        return player && player->team_id == teamId;
                      }) < team_size_) {
      return false;
    }
  }

  return true;
}

bool Game::isRunning() const { return is_running_; }

bool Game::isEmpty() const { return player_sockets_.empty(); }

shared::LobbyUpdate PvPGame::buildLobbyUpdate() const {
  shared::PvPGameLobbyUpdate::TeamInfos teamsInfos{};

  for (std::size_t i{0}; i < team_count_; ++i) {
    teamsInfos[i].emplace();
  }

  std::size_t slot{0};
  for (const auto &teamId : team_ids_) {
    if (!teamId)
      continue;

    auto &tInfo = teamsInfos[slot++];
    tInfo->id = teamId.value();

    if (!tInfo)
      continue;

    forEachPlayerOfTeam(teamId.value(), [&](const auto &player,
                                            std::size_t rank) {
      tInfo->players[rank] = shared::PlayerInfo{
          .id = player.id, .name = player.name, .is_ready = player.is_ready};
    });
  }

  return shared::PvPGameLobbyUpdate{.teams = teamsInfos,
                                    .team_size = team_size_,
                                    .game_started = is_running_};
}

shared::LobbyUpdate CoopGame::buildLobbyUpdate() const {
  shared::CoopGameLobbyUpdate::PlayerInfos playersInfos{};

  shared::iterateOverOptionals(
      players_, [&playersInfos](const auto &player, auto idx) {
        playersInfos[idx] = shared::PlayerInfo{
            .id = player.id, .name = player.name, .is_ready = player.is_ready};
      });

  return shared::CoopGameLobbyUpdate{.players = playersInfos,
                                     .game_started = is_running_};
}

void Game::broadcastLobbyUpdate() const {
  std::visit([this](const auto &u) { broadcast(u); }, buildLobbyUpdate());
}

Game::ClientMessageOutcome
Game::handleClientMessage(const PerSocketPlayers &players_data,
                          std::string_view message) {
  try {
    auto j = shared::decode(message);

    const auto type = j.at("type").get<shared::ClientMessageType>();

    if (!shared::isClientMessageTypeValid(type)) {
      return {
          ClientMessageResult::UnknownType,
          "type out of range: " +
              std::to_string(+static_cast<
                             std::underlying_type_t<shared::ClientMessageType>>(
                  type))};
    }

    switch (type) {
    case shared::ClientMessageType::READY: {
      auto msg = j.at("payload").get<shared::ReadyMessage>();
      setPlayerReady(msg.player_id, msg.is_ready, players_data.ws);
      broadcastLobbyUpdate();
      break;
    }

    case shared::ClientMessageType::PLAYER_INPUT: {
      auto inputs =
          j.at("payload")
              .get<shared::PlayerInputs<shared::MAX_PLAYERS_PER_CLIENT>>();

      for (const auto &input : inputs) {
        if (!input)
          continue;

        auto playerIt = std::find_if(
            players_data.players.begin(), players_data.players.end(),
            [&input](const auto &player) {
              return player && player->first == input->player_id;
            });

        if (playerIt == players_data.players.end())
          continue;

        const auto &player = findPlayerById((*playerIt)->first);

        if (!player)
          continue;

        bool shoot_now = input->buttons & shared::Button::BUTTON_SHOOT;
        if (shoot_now && !player->prev_shoot_held)
          player->pending_shots++;

        player->prev_shoot_held = shoot_now;
        player->pending_movement = static_cast<shared::Button>(
            input->buttons & (shared::BUTTON_LEFT | shared::BUTTON_RIGHT));
      }
      break;
    }

    case shared::ClientMessageType::SWITCH_TEAM: {
      auto g = dynamic_cast<PvPGame *>(this);

      if (!g || is_running_ || is_over_) {
        broadcastLobbyUpdate();
        break;
      }

      const auto msg = j.at("payload").get<shared::SwitchTeamMessage>();

      if (g->countPlayersOfTeam(msg.target_team_id) + 1 > g->getTeamSize()) {
        broadcastLobbyUpdate();
        break;
      }

      for (std::size_t i = 0; i < players_.size(); ++i) {
        auto &player = players_[i];

        if (!player || player->id != msg.player_id ||
            player->ws != players_data.ws)
          continue;

        const auto &teamIds = g->getTeamIds();
        auto targetTeamIt = std::find_if(
            teamIds.begin(), teamIds.end(), [&msg](const auto &teamId) {
              return teamId && teamId.value() == msg.target_team_id;
            });

        if (targetTeamIt == teamIds.end())
          break;

        g->forEachPlayerOfTeam(
            player->team_id.value(),
            [](PlayerConnection &p, std::size_t) { p.is_ready = false; });
        g->forEachPlayerOfTeam(
            msg.target_team_id,
            [](PlayerConnection &p, std::size_t) { p.is_ready = false; });

        auto [begin, end] =
            teamSlotRange(std::distance(teamIds.begin(), targetTeamIt));

        for (std::size_t j = begin; j < end; ++j) {
          if (players_[j])
            continue;

          player->team_id = msg.target_team_id;
          players_[j] = std::move(player);
          players_[i] = std::nullopt;
          break;
        }

        break;
      }

      broadcastLobbyUpdate();
      break;
    }
    }
  } catch (const nlohmann::json::parse_error &e) {
    return {ClientMessageResult::NotJson, e.what()};
  } catch (nlohmann::json::exception &e) {
    return {ClientMessageResult::BadEnvelope, e.what()};
  }

  return {};
}

PlayerConnection *Game::findPlayerById(ServerPlayerId id) {
  auto it =
      std::find_if(players_.begin(), players_.end(), [id](const auto &player) {
        return player && player->id == id;
      });

  if (it != players_.end()) {
    return &(it->value());
  }

  return nullptr;
}

void Game::setPlayerReady(ServerPlayerId id, bool ready, WsConnection *ws) {
  auto player = findPlayerById(id);

  if (player && player->ws == ws) {
    player->is_ready = ready;
  }

  tryStart();
}

void Game::removePlayers(const WsConnection *ws) {
  for (std::size_t i = 0; i < players_.size(); ++i) {
    const auto &player = players_[i];

    if (player && player->ws == ws) {
      std::visit([&player](auto &sim) { sim.removePlayer(player->id); }, sim_);
      players_[i] = std::nullopt;
    }
  }

  player_sockets_.erase(
      std::remove(player_sockets_.begin(), player_sockets_.end(), ws),
      player_sockets_.end());
}

void Game::insertPlayer(
    const std::pair<ServerPlayerId, std::string> &player, WsConnection *ws,
    const std::optional<std::pair<std::size_t, shared::TeamId>> &team) {
  auto [begin, end] = team ? teamSlotRange((*team).first)
                           : std::pair{std::size_t{0}, players_.size()};

  auto slot = begin;
  while (slot < end && players_[slot])
    slot++;

  if (slot >= end)
    return;

  const auto &[playerId, playerName] = player;
  auto &pc = players_[slot].emplace();
  pc.id = playerId;
  pc.ws = ws;
  pc.name = playerName;
  pc.team_id = team ? std::optional((*team).second) : std::nullopt;
}

bool Game::allPlayersReady() const {
  return std::all_of(players_.begin(), players_.end(), [](const auto &player) {
    return !player || player->is_ready;
  });
}

const PvPGame::Teams &PvPGame::getTeamIds() const { return team_ids_; }

const Game::Players &Game::getPlayers() const { return players_; }

std::size_t PvPGame::countPlayersOfTeam(shared::TeamId id) const {
  return std::count_if(
      players_.begin(), players_.end(),
      [&id](const auto &player) { return player && player->team_id == id; });
}

shared::PlayerCount PvPGame::getTeamSize() const { return team_size_; }
