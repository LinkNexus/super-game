#pragma once

#include "network_client.h"
#include "shared/constants.h"
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
  virtual shared::GameState
  step(const std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS>
           &inputs,
       float dt) = 0;
};

enum class LocalMode { SINGLE_PLAYER, DUAL_PLAYER };

class LocalSession : public Session {
public:
  LocalSession(LocalMode mode = LocalMode::SINGLE_PLAYER);
  shared::GameState step(const std::array<std::optional<shared::PlayerInput>,
                                          shared::MAX_PLAYERS> &inputs,
                         float dt) override;
  LocalMode getMode() const;

private:
  LocalMode mode_{LocalMode::SINGLE_PLAYER};
  shared::GameSim sim_{};
  shared::GameState state_{};
};

class OnlineSession : public Session {
public:
  explicit OnlineSession(const std::string &url);
  shared::GameState step(const std::array<std::optional<shared::PlayerInput>,
                                          shared::MAX_PLAYERS> &inputs,
                         float dt) override;
  const shared::LobbyUpdate getLobbyUpdate();
  const uint32_t getPlayerId();
  void sendReady(bool isReady);

private:
  void onMessage(const std::string &msg);
  shared::GameState interpolateState() const;

private:
  NetworkClient client_;
  MailBox<shared::GameState> state_box_{};
  MailBox<shared::LobbyUpdate> lobby_update_box_{};
  MailBox<shared::WelcomeMessage> welcome_message_box_{};
  shared::GameState target_state_{};
  std::optional<shared::GameState> previous_state_{};
  shared::LobbyUpdate lobby_update_{};
  std::chrono::steady_clock::time_point last_update_time_{};
  shared::WelcomeMessage welcome_message_{};
};
