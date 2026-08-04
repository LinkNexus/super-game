#include "session.h"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/messages.h"
#include <algorithm>
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

LocalSession::LocalSession() {
  std::array<uint8_t, shared::MAX_PLAYERS> ids{};
  sim_.start(ids);
}

const shared::GameState &LocalSession::step(const shared::PlayerInput &input,
                                            float dt) {
  std::array<shared::PlayerInput, shared::MAX_PLAYERS> inputs{};
  inputs[0] = input;
  sim_.step(state_, inputs, dt);
  return state_;
}

OnlineSession::OnlineSession(const std::string &url)
    : client_(url, [this](const std::string &msg) { onMessage(msg); }) {
  client_.connect();
}

const shared::GameState &OnlineSession::step(const shared::PlayerInput &input,
                                             float dt) {
  client_.send(nlohmann::json(input).dump());

  if (auto s = state_box_.take())
    state_ = std::move(*s);
  return state_;
}

const shared::LobbyUpdate OnlineSession::getLobbyUpdate() {
  if (auto u = lobby_update_box_.take())
    lobby_update_ = std::move(*u);
  return lobby_update_;
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
  }
}
