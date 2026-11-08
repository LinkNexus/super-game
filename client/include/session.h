#pragma once

#include "constants.h"
#include "mailbox.h"
#include "network_client.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/types.h"
#include <cstddef>
#include <optional>
#include <utility>

struct CoopMode {};

struct PvPMode {
  std::vector<std::pair<shared::TeamCount, shared::PlayerCount>>
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

using ThisMachinePlayerIds = shared::PlayerIds<MAX_PLAYERS_ON_THIS_MACHINE>;
using ThisMachinePlayerInputs =
    shared::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE>;

/// Abstracts over local (in-process) vs online (networked) play so `Game`
/// can drive either the same way each tick.
class Session {
protected:
  shared::PlayerCount player_count_{1};

public:
  virtual ~Session() = default;

  virtual ThisMachinePlayerIds getPlayersIds() = 0;

  virtual shared::GameState step(const ThisMachinePlayerInputs &inputs,
                                 float dt) = 0;
};

struct LocalConfig : Config {};

/// Runs a `GameSim` in-process, with no networking - the same simulation
/// code path the server uses, just fed local input directly.
class LocalSession : public Session {
public:
  LocalSession(const GameMode *mode, shared::PlayerCount player_count);

  shared::GameState
  step(const shared::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE> &inputs,
       float dt) override;

  ThisMachinePlayerIds getPlayersIds() override;

private:
  const GameMode *mode_{nullptr};
  shared::GameSim sim_{};
  shared::GameState state_{};
  ThisMachinePlayerIds player_ids_{};
};

struct OnlineConfig : Config {
  std::array<std::string, shared::MAX_PLAYERS_PER_CLIENT> players_names{};
  std::string server_url{};
};

/// A player's lobby entry plus the index of its team in `teams` (0 in coop).
using LobbyPlayer = std::pair<shared::PlayerInfo, std::size_t>;

/// @return @p id's entry in @p lobby, or nullopt if it isn't listed.
std::optional<LobbyPlayer> findLobbyPlayer(const shared::LobbyUpdate &lobby,
                                           shared::PlayerId id);

class OnlineSession : public Session {
public:
  explicit OnlineSession(const std::string &url,
                         shared::PlayerCount playersCount);

  shared::GameState
  step(const shared::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE> &inputs,
       float dt) override;

  /// @return The most recently received lobby snapshot (player list,
  /// ready states, whether the match has started).
  const shared::LobbyUpdate getLobbyUpdate();

  /// @return The player id assigned by the server's `WelcomeMessage`, or
  /// 0 if none has been received yet.
  ThisMachinePlayerIds getPlayersIds() override;

  /// Readies (or un-readies) one of this machine's players. Ready is per
  /// player (L74); the next lobby update shows the result.
  void sendReady(shared::PlayerId playerId, bool isReady);

  /// Asks to move one of this machine's players to @p teamId. The server
  /// may refuse (team full); the next lobby update shows the result.
  void sendSwitchTeam(shared::PlayerId playerId, shared::TeamId teamId);

  shared::PlayerCount getPlayerCount() const;

private:
  /// Dispatches an incoming `{type, payload}` envelope to the matching
  /// mailbox based on `ServerMessageType`.
  void onMessage(const nlohmann::json &msg);

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
