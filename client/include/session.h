#pragma once

#include "network_client.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <mutex>

class Session {
public:
  virtual ~Session() = default;
  virtual const shared::GameState &step(const shared::PlayerInput &input,
                                        float dt) = 0;
};

class LocalSession : public Session {
public:
  LocalSession();
  const shared::GameState &step(const shared::PlayerInput &input,
                                float dt) override;

private:
  shared::GameSim sim_;
  shared::GameState state_;
};

class OnlineSession : public Session {
public:
  explicit OnlineSession(const std::string &url);
  const shared::GameState &step(const shared::PlayerInput &input,
                                float dt) override;

private:
  void onMessage(const std::string &msg);
  NetworkClient client_;
  shared::GameState state_, pending_state_;
  std::mutex mutex_;
  bool has_pending_state_ = false;
};
