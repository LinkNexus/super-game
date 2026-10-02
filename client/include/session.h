#pragma once

#include "constants.h"
#include "mailbox.h"
#include "network_client.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <cstddef>
#include <optional>

struct CoopMode {
  shared::PlayerCount players_count{1};
};

struct PvPMode {
  std::vector<std::pair<shared::PlayerCount, shared::PlayerCount>>
      pvp_match_ups{};
  std::size_t selected_match_up_idx{0};
};

using GameMode = std::variant<CoopMode, PvPMode>;

struct Config {
  std::size_t mode_idx{};
  std::array<GameMode, 2> modes = {CoopMode{}, PvPMode{}};

  shared::GameState state{};
  shared::GameState prev_state{};

  shared::PlayerCount player_count{1};

  virtual ~Config() = default;
};

/// Abstracts over local (in-process) vs online (networked) play so `Game`
/// can drive either the same way each tick.
class Session {
protected:
  shared::PlayerCount player_count_{1};

public:
  virtual ~Session() = default;

  virtual std::array<shared::PlayerId, MAX_PLAYERS_ON_THIS_MACHINE>
  getPlayersIds() = 0;

  virtual shared::GameState
  step(const std::array<shared::PlayerInput, MAX_PLAYERS_ON_THIS_MACHINE>
           &inputs,
       float dt) = 0;
};

struct LocalConfig : Config {};

/// Runs a `GameSim` in-process, with no networking - the same simulation
/// code path the server uses, just fed local input directly.
class LocalSession : public Session {
public:
  LocalSession(const GameMode *mode, shared::PlayerCount player_count);

  shared::GameState step(const std::array<shared::PlayerInput,
                                          MAX_PLAYERS_ON_THIS_MACHINE> &inputs,
                         float dt) override;

  std::array<shared::PlayerId, MAX_PLAYERS_ON_THIS_MACHINE>
  getPlayersIds() override;

private:
  const GameMode *mode_{nullptr};
  shared::GameSim sim_{};
  shared::GameState state_{};
  std::array<shared::PlayerId, MAX_PLAYERS_ON_THIS_MACHINE> player_ids_{};
};

struct OnlineConfig : Config {
  std::array<std::string, shared::MAX_PLAYERS_PER_CLIENT> players_names{};
  bool are_ready{};
  std::string server_url{};
};

class OnlineSession : public Session {
public:
  explicit OnlineSession(const std::string &url,
                         shared::PlayerCount playersCount);

  shared::GameState step(const std::array<shared::PlayerInput,
                                          MAX_PLAYERS_ON_THIS_MACHINE> &inputs,
                         float dt) override;

  /// @return The most recently received lobby snapshot (player list,
  /// ready states, whether the match has started).
  const shared::LobbyUpdate getLobbyUpdate();

  /// @return The player id assigned by the server's `WelcomeMessage`, or
  /// 0 if none has been received yet.
  std::array<shared::PlayerId, MAX_PLAYERS_ON_THIS_MACHINE>
  getPlayersIds() override;

  /// Sends this client's ready-state toggle to the server.
  void sendReady(bool isReady);

  shared::PlayerCount getPlayerCount() const;

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
