#include "session.h"
#include "constants.h"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/wire.h"
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <variant>

LocalSession::LocalSession(const GameMode *mode,
                           shared::PlayerCount player_count) {
  mode_ = mode;
  player_count_ = player_count;

  std::visit(shared::overloaded{
                 [&](const CoopMode &) {
                   auto &s = sim_.emplace<shared::CoopGameSim>();

                   for (shared::PlayerCount i = 0; i < player_count; ++i) {
                     auto id = shared::PlayerId(i + 1);
                     player_ids_[i] = id;
                   }

                   s.start(player_ids_);

                   state_ = shared::CoopGameState();
                 },
                 [&](const PvPMode &pvpMode) {
                   auto &s = sim_.emplace<shared::PvPGameSim>();

                   const auto &[team_count, team_size] =
                       pvpMode.pvp_match_ups[pvpMode.selected_match_up_idx];

                   shared::PvPGameSim::TeamRosters ids{};
                   shared::PlayerId currentId{1};

                   for (std::size_t i = 0; i < team_count; ++i) {
                     auto &[rId, rPlayers] = ids[i].emplace();
                     rId = i + 1;

                     for (std::size_t j = 0; j < team_size; ++j) {
                       rPlayers[j] = currentId++;
                       player_ids_[i * team_size + j] = rPlayers[j];
                     }
                   }

                   s.start(ids);
                   state_ = shared::PvPGameState();
                 }},
             *mode);
}

shared::GameState LocalSession::step(
    const shared::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE> &inputs, float dt) {
  std::visit(shared::overloaded{
                 [&](shared::CoopGameSim &sim, shared::CoopGameState &state) {
                   sim.step(state, inputs, dt);
                 },
                 [&](shared::PvPGameSim &sim, shared::PvPGameState &state) {
                   sim.step(state, inputs, dt);
                 },
                 [](auto &, auto &) {
                   throw std::runtime_error("Invalid sim/state variant");
                 }},
             sim_, state_);

  return state_;
}

OnlineSession::OnlineSession(const std::string &url,
                             shared::PlayerCount playerCount)
    : client_(url, [this](const nlohmann::json &msg) { onMessage(msg); }) {
  player_count_ = playerCount;
  client_.connect();
}

shared::GameState OnlineSession::step(
    const shared::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE> &inputs, float) {
  nlohmann::json inputEnvelope;
  inputEnvelope["type"] = shared::ClientMessageType::PLAYER_INPUT;

  shared::PlayerInputs<shared::MAX_PLAYERS_PER_CLIENT> inputsToSend{};

  for (std::size_t i{0}; i < inputs.size() && i < inputsToSend.size(); ++i) {
    inputsToSend[i] = inputs[i];
  }

  inputEnvelope["payload"] = inputsToSend;
  client_.send(inputEnvelope);

  if (auto s = state_box_.take()) {
    last_update_time_ = std::chrono::steady_clock::now();
    previous_state_ = std::move(target_state_);
    target_state_ = std::move(*s);
  }
  return interpolateState();
}

const shared::LobbyUpdate OnlineSession::getLobbyUpdate() {
  if (auto u = lobby_update_box_.take())
    lobby_update_ = std::move(*u);
  return lobby_update_;
}

ThisMachinePlayerIds OnlineSession::getPlayersIds() {
  if (auto msg = welcome_message_box_.take())
    welcome_message_ = std::move(*msg);

  ThisMachinePlayerIds ids{};
  for (std::size_t i = 0;
       i < ids.size() && i < welcome_message_.players_ids.size(); ++i) {
    ids[i] = welcome_message_.players_ids[i];
  }

  return ids;
}

void OnlineSession::sendReady(shared::PlayerId playerId, bool isReady) {
  nlohmann::json readyEnvelope;
  readyEnvelope["type"] = shared::ClientMessageType::READY;
  readyEnvelope["payload"] =
      shared::ReadyMessage{.player_id = playerId, .is_ready = isReady};

  client_.send(readyEnvelope);
}

void OnlineSession::sendSwitchTeam(shared::PlayerId playerId,
                                   shared::TeamId teamId) {
  nlohmann::json switchEnvelope;
  switchEnvelope["type"] = shared::ClientMessageType::SWITCH_TEAM;
  switchEnvelope["payload"] = shared::SwitchTeamMessage{
      .player_id = playerId, .target_team_id = teamId};

  client_.send(switchEnvelope);
}

void OnlineSession::onMessage(const nlohmann::json &j) {
  try {
    auto payload = j.at("payload");

    switch (j.at("type").get<shared::ServerMessageType>()) {
    case shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE:
      lobby_update_box_.set(payload.get<shared::CoopGameLobbyUpdate>());
      break;
    case shared::ServerMessageType::PVP_GAME_LOBBY_UPDATE:
      lobby_update_box_.set(payload.get<shared::PvPGameLobbyUpdate>());
      break;
    case shared::ServerMessageType::COOP_GAME_STATE:
      state_box_.set(payload.get<shared::CoopGameState>());
      break;
    case shared::ServerMessageType::PVP_GAME_STATE:
      state_box_.set(payload.get<shared::PvPGameState>());
      break;
    case shared::ServerMessageType::WELCOME:
      welcome_message_box_.set(payload.get<shared::WelcomeMessage>());
      break;
    }
  } catch (const nlohmann::json::exception &e) {
    return;
  }
}

shared::GameState OnlineSession::interpolateState() const {
  if (!previous_state_.has_value())
    return target_state_;

  shared::GameState interpolatedState = target_state_;
  float alpha =
      std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now() -
                                              last_update_time_)
                         .count() /
                     shared::FIXED_DT,
                 0.0f, 1.0f);

  auto findIt = [](auto &container, auto id) {
    auto it = std::find_if(container.begin(), container.end(),
                           [&id](const auto &p) { return p && p->id == id; });

    return std::pair{it != container.end(), it};
  };

  std::visit(
      shared::overloaded{
          [&alpha, &findIt](const shared::CoopGameState &prevS,
                            const shared::CoopGameState &targetS,
                            shared::CoopGameState &interS) {
            for (const auto &targetP : targetS.players) {
              if (targetP) {
                auto [res, previousPlayerIt] =
                    findIt(prevS.players, targetP->id);

                if (res) {
                  auto [_, interPlayerIt] = findIt(interS.players, targetP->id);

                  (*interPlayerIt)->position =
                      (*previousPlayerIt)
                          ->position.lerpedWith(targetP->position, alpha);
                }
              }
            }

            interS.boss.position =
                prevS.boss.position.lerpedWith(targetS.boss.position, alpha);
            interS.enemies_offset =
                prevS.enemies_offset.lerpedWith(targetS.enemies_offset, alpha);
          },
          [&alpha, &findIt](const shared::PvPGameState &prevS,
                            const shared::PvPGameState &targetS,
                            shared::PvPGameState &interS) {
            for (const auto &targetTeam : targetS.teams) {
              if (targetTeam) {
                auto [res, prevTeamIt] = findIt(prevS.teams, targetTeam->id);

                if (!res)
                  continue;

                auto [_, interTeamIt] = findIt(interS.teams, targetTeam->id);

                for (const auto &targetPlayer : targetTeam->players) {
                  auto [res, prevPlayerIt] =
                      findIt((*prevTeamIt)->players, targetPlayer->id);

                  if (res) {
                    auto [_, interPlayerIt] =
                        findIt((*interTeamIt)->players, targetPlayer->id);

                    (*interPlayerIt)->position =
                        (*prevPlayerIt)
                            ->position.lerpedWith(targetPlayer->position,
                                                  alpha);
                  }
                }
              }
            }
          },
          [](auto &, auto &, auto &) {}},
      previous_state_.value(), target_state_, interpolatedState);

  auto lerpBullets = [](const shared::CommonGameState &prevS,
                        const shared::CommonGameState &targetS,
                        shared::CommonGameState &interS, float alpha) {
    for (std::size_t idx = 0; idx < interS.bullets.size(); ++idx) {
      auto &targetBullet = targetS.bullets[idx];
      auto &previousBullet = prevS.bullets[idx];

      if (targetBullet.active && previousBullet.active)
        interS.bullets[idx].position =
            previousBullet.position.lerpedWith(targetBullet.position, alpha);
    }
  };

  std::visit(shared::overloaded{
                 [&alpha, &lerpBullets](const shared::CoopGameState &prevS,
                                        const shared::CoopGameState &targetS,
                                        shared::CoopGameState &interS) {
                   lerpBullets(prevS, targetS, interS, alpha);
                 },
                 [&alpha, &lerpBullets](const shared::PvPGameState &prevS,
                                        const shared::PvPGameState &targetS,
                                        shared::PvPGameState &interS) {
                   lerpBullets(prevS, targetS, interS, alpha);
                 },
                 [](auto &, auto &, auto &) {}},
             previous_state_.value(), target_state_, interpolatedState);

  return interpolatedState;
}

ThisMachinePlayerIds LocalSession::getPlayersIds() { return player_ids_; }

shared::PlayerCount OnlineSession::getPlayerCount() const {
  return player_count_;
}

std::optional<LobbyPlayer> findLobbyPlayer(const shared::LobbyUpdate &lobby,
                                           shared::PlayerId id) {
  const auto inSlots =
      [id](const auto &slots) -> std::optional<shared::PlayerInfo> {
    for (const auto &p : slots)
      if (p && p->id == id)
        return p;
    return std::nullopt;
  };

  return std::visit(
      shared::overloaded{[&](const shared::CoopGameLobbyUpdate &u)
                             -> std::optional<LobbyPlayer> {
                           if (auto p = inSlots(u.players))
                             return std::pair{*p, std::size_t{0}};
                           return std::nullopt;
                         },
                         [&](const shared::PvPGameLobbyUpdate &u)
                             -> std::optional<LobbyPlayer> {
                           for (std::size_t t = 0; t < u.teams.size(); ++t) {
                             if (!u.teams[t])
                               continue;
                             if (auto p = inSlots(u.teams[t]->players))
                               return std::pair{*p, t};
                           }
                           return std::nullopt;
                         }},
      lobby);
}
