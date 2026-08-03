#include "session.h"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/messages.h"
#include <mutex>
#include <string>

LocalSession::LocalSession() {
  std::array<uint8_t, MAX_PLAYERS> ids{};
  sim_.start(ids);
}

const shared::GameState &LocalSession::step(const shared::PlayerInput &input,
                                            float dt) {
  std::array<shared::PlayerInput, MAX_PLAYERS> inputs{};
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
  std::lock_guard<std::mutex> lock(mutex_);
  if (has_pending_state_) {
    state_ = pending_state_;
    has_pending_state_ = false;
  }

  client_.send(nlohmann::json(input).dump());
  return state_;
}

void OnlineSession::onMessage(const std::string &msg) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto j = nlohmann::json::parse(msg);

  switch (j.at("type").get<shared::MessageType>()) {
  case shared::MessageType::LOBBY_UPDATE:
    break;
  case shared::MessageType::GAME_STATE:
    pending_state_ = j.at("payload").get<shared::GameState>();
    break;
  }

  has_pending_state_ = true;
}
