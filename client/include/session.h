#pragma once

#include "network_client.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <mutex>
#include <optional>

/// Single-slot, thread-safe mailbox used to hand a value from
/// `NetworkClient`'s background IXWebSocket thread to the main thread
/// polling it once per frame. A new `set()` overwrites any unread pending
/// value - only the latest matters for rendering.
template <typename T> class MailBox {
public:
  void set(T value) {
    std::lock_guard<std::mutex> lock(mutex_);
    pending_ = std::move(value);
    has_pending_ = true;
  }

  /// @return The pending value and clears it, or `std::nullopt` if nothing
  /// new has arrived since the last call.
  std::optional<T> take() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!has_pending_)
      return std::nullopt;
    has_pending_ = false;
    return std::move(pending_);
  }

private:
  std::mutex mutex_;
  T pending_;
  bool has_pending_;
};

/// Abstracts over local (in-process) vs online (networked) play so `Game`
/// can drive either the same way each tick.
class Session {
public:
  virtual ~Session() = default;

  /// Advances the session by one tick with the given local @p inputs and
  /// @p dt, returning the resulting `GameState` to render.
  virtual shared::GameState
  step(const std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS>
           &inputs,
       float dt) = 0;
};

/// Runs a `GameSim` in-process, with no networking - the same simulation
/// code path the server uses, just fed local input directly.
class LocalSession : public Session {
public:
  enum class Mode { SINGLE_PLAYER, DUAL_PLAYER, PvP };
  struct Config {
    int mode_idx{};
    static constexpr std::array<Mode, 3> modes = {Mode::SINGLE_PLAYER,
                                                  Mode::DUAL_PLAYER, Mode::PvP};
  };

  LocalSession(Mode mode);
  Mode getMode() const;
  shared::GameState step(const std::array<std::optional<shared::PlayerInput>,
                                          shared::MAX_PLAYERS> &inputs,
                         float dt) override;

private:
  Mode mode_{Mode::SINGLE_PLAYER};
  shared::GameSim sim_{};
  shared::GameState state_{};
};

enum class OnlineMode { COOP, _1V1, _2V2 };

class OnlineSession : public Session {
public:
  struct Config {
    char player_name[shared::MAX_NAME_LENGTH + 1]{};
    bool is_ready{};
    std::string server_url;

    int mode_idx;
    constexpr static std::array<OnlineMode, 3> modes = {
        OnlineMode::COOP, OnlineMode::_1V1, OnlineMode::_2V2};
  };

  explicit OnlineSession(const std::string &url);
  shared::GameState step(const std::array<std::optional<shared::PlayerInput>,
                                          shared::MAX_PLAYERS> &inputs,
                         float dt) override;

  /// @return The most recently received lobby snapshot (player list,
  /// ready states, whether the match has started).
  const shared::LobbyUpdate getLobbyUpdate();

  /// @return The player id assigned by the server's `WelcomeMessage`, or
  /// 0 if none has been received yet.
  const uint32_t getPlayerId();

  /// Sends this client's ready-state toggle to the server.
  void sendReady(bool isReady);

private:
  /// Dispatches an incoming `{type, payload}` envelope to the matching
  /// mailbox based on `ServerMessageType`.
  void onMessage(const std::string &msg);

  /// @return `target_state_` lerped from `previous_state_` based on
  /// elapsed time since the last snapshot, vs. `shared::FIXED_DT`.
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
