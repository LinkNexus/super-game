#pragma once

#include "network_client.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <mutex>
#include <optional>

template <typename T> class MailBox {
public:
  void set(T value);

  std::optional<T> take();

private:
  std::mutex mutex_;
  T pending_;
  bool has_pending_;
};

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
  const shared::LobbyUpdate getLobbyUpdate();

private:
  void onMessage(const std::string &msg);

private:
  NetworkClient client_;
  MailBox<shared::GameState> state_box_;
  MailBox<shared::LobbyUpdate> lobby_update_box_;
  shared::GameState state_;
  shared::LobbyUpdate lobby_update_;
};
