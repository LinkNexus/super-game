#include "session.h"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/messages.h"
#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

template <typename T> void MailBox<T>::set(T value) {
  std::lock_guard<std::mutex> lock(mutex_);
  pending_ = std::move(value);
  has_pending_ = true;
}

template <typename T> std::optional<T> MailBox<T>::take() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!has_pending_)
    return std::nullopt;
  has_pending_ = false;
  return std::move(pending_);
}

LocalSession::LocalSession(LocalMode mode) {
  mode_ = mode;

  std::array<std::optional<uint8_t>, shared::MAX_PLAYERS> ids{};
  ids[0] = 1;
  if (mode_ == LocalMode::DUAL_PLAYER)
    ids[1] = 2;

  sim_.start(ids);
}

LocalMode LocalSession::getMode() const { return mode_; }

shared::GameState LocalSession::step(
    const std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS>
        &inputs,
    float dt) {
  sim_.step(state_, inputs, dt);
  return state_;
}

OnlineSession::OnlineSession(const std::string &url)
    : client_(url, [this](const std::string &msg) { onMessage(msg); }) {
  client_.connect();
}

shared::GameState OnlineSession::step(
    const std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS>
        &inputs,
    float dt) {
  client_.send(nlohmann::json(inputs[0]).dump());

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

void OnlineSession::onMessage(const std::string &msg) {
  auto j = nlohmann::json::parse(msg);
  auto payload = j.at("payload");

  switch (j.at("type").get<shared::MessageType>()) {
  case shared::MessageType::LOBBY_UPDATE:
    lobby_update_box_.set(payload.get<shared::LobbyUpdate>());
    break;
  case shared::MessageType::GAME_STATE:
    state_box_.set(payload.get<shared::GameState>());
    break;
  case shared::MessageType::WELCOME:
    welcome_message_box_.set(payload.get<shared::WelcomeMessage>());
    break;
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

  for (std::size_t idx = 0; idx < interpolatedState.players.size(); ++idx) {
    auto &targetPlayer = target_state_.players[idx];
    auto &previousPlayer = previous_state_->players[idx];

    if (!targetPlayer.has_value() || !previousPlayer.has_value())
      continue;

    interpolatedState.players[idx]->position =
        previousPlayer->position.lerp(targetPlayer->position, alpha);
  }

  for (std::size_t idx = 0; idx < interpolatedState.bullets.size(); ++idx) {
    auto &targetBullet = target_state_.bullets[idx];
    auto &previousBullet = previous_state_->bullets[idx];
    interpolatedState.bullets[idx].position =
        previousBullet.position.lerp(targetBullet.position, alpha);
  }

  interpolatedState.boss.position =
      previous_state_->boss.position.lerp(target_state_.boss.position, alpha);

  interpolatedState.enemies_offset_x =
      previous_state_->enemies_offset_x +
      (target_state_.enemies_offset_x - previous_state_->enemies_offset_x) *
          alpha;
  interpolatedState.enemies_offset_y =
      previous_state_->enemies_offset_y +
      (target_state_.enemies_offset_y - previous_state_->enemies_offset_y) *
          alpha;

  return interpolatedState;
}
