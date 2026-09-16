#include "session.h"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

LocalSession::LocalSession(Mode mode) : mode_(mode) {
  mode_ = mode;

  switch (mode) {
  case Mode::SINGLE_PLAYER:
  case Mode::DUAL_PLAYER: {
    std::array<std::optional<uint32_t>, shared::MAX_PLAYERS> ids{};
    ids[0] = 1;
    if (mode == Mode::DUAL_PLAYER)
      ids[1] = 2;

    auto &s = sim_.emplace<shared::CoopGameSim>();
    s.start(ids);
    state_ = shared::CoopGameState();
    break;
  }

  case Mode::PvP:
    shared::PvPGameSim::PerTeamPlayerIds ids{};
    ids[0][0] = 1;
    ids[1][0] = 2;

    auto &s = sim_.emplace<shared::PvPGameSim>(1);
    s.start(ids);
    state_ = shared::PvPGameState();
    break;
  }
}

shared::GameState LocalSession::step(
    const std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS>
        &inputs,
    float dt) {
  std::visit(shared::overloaded{
                 [&](shared::CoopGameSim &sim, shared::CoopGameState &state) {
                   sim.step(state, inputs, dt);
                 },
                 [&](shared::PvPGameSim &sim, shared::PvPGameState &state) {
                   sim.step(state, inputs, dt);
                 },
                 [&](auto &, auto &) {
                   throw std::runtime_error("Invalid sim state");
                 }},
             sim_, state_);
  return state_;
}

LocalSession::Mode LocalSession::getMode() const { return mode_; }

OnlineSession::OnlineSession(const std::string &url)
    : client_(url, [this](const std::string &msg) { onMessage(msg); }) {
  client_.connect();
}

shared::GameState OnlineSession::step(
    const std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS>
        &inputs,
    float dt) {
  nlohmann::json inputEnvelope;
  inputEnvelope["type"] = shared::ClientMessageType::PLAYER_INPUT;
  inputEnvelope["payload"] = inputs[0];
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

const uint32_t OnlineSession::getPlayerId() {
  if (auto msg = welcome_message_box_.take())
    welcome_message_ = std::move(*msg);
  return welcome_message_.player_id;
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
          [&alpha](const shared::CoopGameState &prevS,
                   const shared::CoopGameState &targetS,
                   shared::CoopGameState &interS) {
            for (std::size_t idx = 0; idx < interS.players.size(); ++idx) {
              auto &targetPlayer = targetS.players[idx];
              auto &previousPlayer = prevS.players[idx];

              if (!targetPlayer.has_value() || !previousPlayer.has_value())
                continue;

              interS.players[idx]->position =
                  previousPlayer->position.lerp(targetPlayer->position, alpha);
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
          [](const shared::PvPGameState &prevS,
             const shared::PvPGameState &targetS,
             shared::PvPGameState &interS) {

          },
          [](auto &, auto &, auto &) {}},
      previous_state_.value(), target_state_, interpolatedState);

  std::visit(shared::overloaded{[&alpha](const auto &prevS, const auto &targetS,
                                         auto &interS) {
               for (std::size_t idx = 0; idx < interS.bullets.size(); ++idx) {
                 auto &targetBullet = targetS.bullets[idx];
                 auto &previousBullet = prevS.bullets[idx];
                 interS.bullets[idx].position =
                     previousBullet.position.lerp(targetBullet.position, alpha);
               }
             }},
             previous_state_.value(), target_state_, interpolatedState);

  return interpolatedState;
}
