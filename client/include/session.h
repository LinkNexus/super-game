#pragma once

#include "constants.h"
#include "mailbox.h"
#include "network_client.h"
#include "shared/helpers.h"
#include "shared/sim/constants.h"
#include "shared/sim/messages.h"
#include "shared/sim/modes.h"
#include "shared/sim/types.h"
#include <cstddef>
#include <optional>
#include <utility>

struct CoopMode {};

struct PvPMode {
  std::vector<std::pair<shared::sim::TeamCount, shared::sim::PlayerCount>> pvp_match_ups{};
  std::size_t selected_match_up_idx{0};
};

using GameMode = std::variant<CoopMode, PvPMode>;

struct Config {
  std::size_t mode_idx{};
  std::array<GameMode, 2> modes = {CoopMode{}, PvPMode{}};

  shared::sim::GameState state{};
  shared::sim::GameState prev_state{};

  shared::sim::PlayerCount player_count{1};

  virtual ~Config() = default;
};

using ThisMachinePlayerIds = shared::sim::PlayerIds<MAX_PLAYERS_ON_THIS_MACHINE>;
using ThisMachinePlayerInputs = shared::sim::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE>;

/// Abstracts over local (in-process) vs online (networked) play so `Game`
/// can drive either the same way each tick.
class Session {
protected:
  shared::sim::PlayerCount player_count_{1};

public:
  virtual ~Session() = default;

  virtual ThisMachinePlayerIds getPlayersIds() = 0;

  virtual shared::sim::GameState step(const ThisMachinePlayerInputs &inputs, float dt) = 0;
};

struct LocalConfig : Config {};

/// Runs a `GameSim` in-process, with no networking - the same simulation
/// code path the server uses, just fed local input directly.
template <shared::sim::World W>
class LocalSession : public Session {
public:
  LocalSession(const GameMode *mode, shared::sim::PlayerCount playerCount) {
    mode_ = mode;
    player_count_ = playerCount;

    std::visit(
        shared::overloaded{
            [&](const CoopMode &) {
              auto &s = sim_.emplace<shared::sim::FlatCoop>();

              for (std::size_t i = 0; i < playerCount; ++i) {
                auto id = shared::sim::PlayerId(i + 1);
                player_ids_[i] = id;
              }

              s.start(player_ids_);

              state_ = shared::sim::CoopGameState();
            },
            [&](const PvPMode &pvpMode) {
              auto &s = sim_.emplace<shared::sim::FlatPvP>();

              const auto &[team_count, team_size] =
                  pvpMode.pvp_match_ups[pvpMode.selected_match_up_idx];

              shared::sim::PvPGameTeamRosters ids{};
              shared::sim::PlayerId currentId{1};

              for (std::size_t i = 0; i < team_count; ++i) {
                auto &[rId, rPlayers] = ids[i].emplace();
                rId = i + 1;

                for (std::size_t j = 0; j < team_size; ++j) {
                  rPlayers[j] = currentId++;
                  player_ids_[i * team_size + j] = rPlayers[j];
                }
              }

              s.start(ids);
              state_ = shared::sim::PvPGameState();
            }
        },
        *mode
    );
  }

  shared::sim::GameState
  step(const shared::sim::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE> &inputs, float dt) override {
    std::visit(
        shared::overloaded{
            [&](shared::sim::CoopGameSim<W> &sim, shared::sim::CoopGameState &state) {
              sim.step(state, inputs, dt);
            },
            [&](shared::sim::PvPGameSim<W> &sim, shared::sim::PvPGameState &state) {
              sim.step(state, inputs, dt);
            },
            [](auto &, auto &) { throw std::runtime_error("Invalid sim/state variant"); }
        },
        sim_,
        state_
    );

    return state_;
  }

  ThisMachinePlayerIds getPlayersIds() override { return player_ids_; }

private:
  const GameMode *mode_{nullptr};
  shared::sim::GameSim sim_{};
  shared::sim::GameState state_{};
  ThisMachinePlayerIds player_ids_{};
};

struct OnlineConfig : Config {
  std::array<std::string, shared::sim::MAX_PLAYERS_PER_CLIENT> players_names{};
  std::string server_url{};
};

/// A player's lobby entry plus the index of its team in `teams` (0 in coop).
using LobbyPlayer = std::pair<shared::sim::PlayerInfo, std::size_t>;

/// @return @p id's entry in @p lobby, or nullopt if it isn't listed.
std::optional<LobbyPlayer>
findLobbyPlayer(const shared::sim::LobbyUpdate &lobby, shared::sim::PlayerId id);

class OnlineSession : public Session {
public:
  explicit OnlineSession(const std::string &url, shared::sim::PlayerCount playersCount);

  shared::sim::GameState
  step(const shared::sim::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE> &inputs, float dt) override;

  /// @return The most recently received lobby snapshot (player list,
  /// ready states, whether the match has started).
  const shared::sim::LobbyUpdate getLobbyUpdate();

  /// @return The player id assigned by the server's `WelcomeMessage`, or
  /// 0 if none has been received yet.
  ThisMachinePlayerIds getPlayersIds() override;

  /// Readies (or un-readies) one of this machine's players. Ready is per
  /// player (L74); the next lobby update shows the result.
  void sendReady(shared::sim::PlayerId playerId, bool isReady);

  /// Asks to move one of this machine's players to @p teamId. The server
  /// may refuse (team full); the next lobby update shows the result.
  void sendSwitchTeam(shared::sim::PlayerId playerId, shared::sim::TeamId teamId);

  shared::sim::PlayerCount getPlayerCount() const;

private:
  /// Dispatches an incoming `{type, payload}` envelope to the matching
  /// mailbox based on `ServerMessageType`.
  void onMessage(const nlohmann::json &msg);

  /// @return `target_state_` lerped from `previous_state_` based on
  /// elapsed time since the last snapshot, vs. `shared::FIXED_DT`.
  shared::sim::GameState interpolateState() const;

private:
  NetworkClient client_;
  MailBox<shared::sim::GameState> state_box_{};
  MailBox<shared::sim::LobbyUpdate> lobby_update_box_{};
  MailBox<shared::sim::WelcomeMessage> welcome_message_box_{};
  shared::sim::GameState target_state_{};
  std::optional<shared::sim::GameState> previous_state_{};
  shared::sim::LobbyUpdate lobby_update_{};
  std::chrono::steady_clock::time_point last_update_time_{};
  shared::sim::WelcomeMessage welcome_message_{};
};
