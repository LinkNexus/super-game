#include "session.h"
#include "constants.h"
#include "nlohmann/json.hpp"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <algorithm>
#include <optional>
#include <string>
#include <utility>

LocalSession::LocalSession(const GameMode *mode,
                           shared::PlayerCount player_count) {
  mode_ = mode;
  player_count_ = player_count;

  std::visit(shared::overloaded{
                 [&](const CoopMode &coopMode) {
                   auto &s = sim_.emplace<shared::CoopGameSim>();
                   s.init(coopMode.players_count);
                   auto ids = s.start();
                   for (std::size_t i = 0; i < coopMode.players_count; ++i) {
                     player_ids_[i] = ids[i];
                   }

                   state_ = shared::CoopGameState();
                 },
                 [&](const PvPMode &pvpMode) {
                   auto &s = sim_.emplace<shared::PvPGameSim>();

                   auto &[team_count, team_size] =
                       pvpMode.pvp_match_ups[pvpMode.selected_match_up_idx];

                   s.init(team_size, team_count);
                   auto ids = s.start();

                   for (std::size_t team = 0; team < team_count; ++team) {
                     for (std::size_t player = 0; player < team_size;
                          ++player) {
                       std::size_t idx = team * team_size + player;
                       player_ids_[idx] = ids[team][player];
                     }
                   }

                   state_ = shared::PvPGameState();
                 }},
             *mode);
}

shared::GameState LocalSession::step(
    const std::array<shared::PlayerInput, MAX_PLAYERS_ON_THIS_MACHINE> &inputs,
    float dt) {
  std::visit(
      shared::overloaded{
          [&](const CoopMode &coopMode) {
            std::array<shared::PlayerInput, shared::MAX_PLAYERS_COOP>
                simInputs{};

            for (std::size_t i = 0; i < coopMode.players_count; ++i) {
              simInputs[i] = inputs[i];
            }

            auto &s = std::get<shared::CoopGameSim>(sim_);
            s.step(std::get<shared::CoopGameState>(state_), simInputs, dt);
          },
          [&](const PvPMode &pvpMode) {
            std::array<shared::PlayerInput,
                       shared::MAX_TEAMS * shared::MAX_PLAYERS_PER_TEAM>
                simInputs{};

            auto &[team_count, team_size] =
                pvpMode.pvp_match_ups[pvpMode.selected_match_up_idx];

            for (std::size_t i = 0; i < team_count * team_size; ++i) {
              simInputs[i] = inputs[i];
            }

            auto &s = std::get<shared::PvPGameSim>(sim_);
            s.step(std::get<shared::PvPGameState>(state_), simInputs, dt);
          }},
      *mode_);

  return state_;
}

OnlineSession::OnlineSession(const std::string &url,
                             shared::PlayerCount playerCount)
    : client_(url, [this](const std::string &msg) { onMessage(msg); }) {
  player_count_ = playerCount;
  client_.connect();
}

shared::GameState OnlineSession::step(
    const std::array<shared::PlayerInput, MAX_PLAYERS_ON_THIS_MACHINE> &inputs,
    float dt) {
  nlohmann::json inputEnvelope;
  inputEnvelope["type"] = shared::ClientMessageType::PLAYER_INPUT;

  std::array<shared::PlayerInput, shared::MAX_PLAYERS_PER_CLIENT> payload{};

  for (std::size_t i = 0; i < player_count_; ++i) {
    payload[i] = inputs[i];
  }

  inputEnvelope["payload"] = payload;
  client_.send(inputEnvelope.dump());

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

std::array<shared::PlayerId, MAX_PLAYERS_ON_THIS_MACHINE>
OnlineSession::getPlayersIds() {
  if (auto msg = welcome_message_box_.take())
    welcome_message_ = std::move(*msg);

  if (MAX_PLAYERS_ON_THIS_MACHINE == shared::MAX_PLAYERS_PER_CLIENT)
    return welcome_message_.players_ids;
  else {
    std::array<shared::PlayerId, MAX_PLAYERS_ON_THIS_MACHINE> players_ids{};

    for (std::size_t i = 0; i < player_count_; ++i) {
      players_ids[i] = welcome_message_.players_ids[i];
    }

    return players_ids;
  }
}

void OnlineSession::sendReady(bool isReady) {
  nlohmann::json readyEnvelope;
  readyEnvelope["type"] = shared::ClientMessageType::READY;
  readyEnvelope["payload"] = shared::ReadyMessage{.is_ready = isReady};

  client_.send(readyEnvelope.dump());
}

void OnlineSession::onMessage(const std::string &msg) {
  try {
    auto j = nlohmann::json::parse(msg);
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

  std::visit(
      shared::overloaded{
          [this, &alpha](const shared::CoopGameState &prevS,
                         const shared::CoopGameState &targetS,
                         shared::CoopGameState &interS) {
            for (std::size_t i = 0; i < interS.player_count; ++i) {
              auto &targetPlayer = targetS.players[i];
              auto &previousPlayer = prevS.players[i];

              interS.players[i].position =
                  previousPlayer.position.lerp(targetPlayer.position, alpha);
            }

            interS.boss.position =
                prevS.boss.position.lerp(targetS.boss.position, alpha);

            interS.enemies_offset_x =
                prevS.enemies_offset_x +
                (targetS.enemies_offset_x - prevS.enemies_offset_x) * alpha;
            interS.enemies_offset_y =
                prevS.enemies_offset_y +
                (targetS.enemies_offset_y - prevS.enemies_offset_y) * alpha;
          },
          [&alpha](const shared::PvPGameState &prevS,
                   const shared::PvPGameState &targetS,
                   shared::PvPGameState &interS) {
            for (std::size_t teamIdx = 0; teamIdx < interS.teams_count;
                 ++teamIdx) {
              for (std::size_t playerIdx = 0;
                   playerIdx < interS.teams[teamIdx].size; ++playerIdx) {
                auto &targetPlayer = targetS.teams[teamIdx].players[playerIdx];
                auto &previousPlayer = prevS.teams[teamIdx].players[playerIdx];

                interS.teams[teamIdx].players[playerIdx].position =
                    previousPlayer.position.lerp(targetPlayer.position, alpha);
              }
            }
          },
          [](auto &, auto &, auto &) {}},
      previous_state_.value(), target_state_, interpolatedState);

  auto lerpBullets = [](const shared::BaseState &prevS,
                        const shared::BaseState &targetS,
                        shared::BaseState &interS, float alpha) {
    for (std::size_t idx = 0; idx < interS.bullets.size(); ++idx) {
      auto &targetBullet = targetS.bullets[idx];
      auto &previousBullet = prevS.bullets[idx];
      interS.bullets[idx].position =
          previousBullet.position.lerp(targetBullet.position, alpha);
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

std::array<shared::PlayerId, MAX_PLAYERS_ON_THIS_MACHINE>
LocalSession::getPlayersIds() {
  return player_ids_;
}

shared::PlayerCount OnlineSession::getPlayerCount() const {
  return player_count_;
}
