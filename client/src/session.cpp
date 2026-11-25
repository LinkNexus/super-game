#include "session.h"
#include "constants.h"
#include "nlohmann/json.hpp"
#include "shared/helpers.h"
#include "shared/sim/constants.h"
#include "shared/sim/messages.h"
#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <variant>

OnlineSession::OnlineSession(const std::string &url, shared::sim::PlayerCount playerCount)
    : client_(url, [this](const nlohmann::json &msg) { onMessage(msg); }) {
  player_count_ = playerCount;
  client_.connect();
}

shared::sim::GameState
OnlineSession::step(const shared::sim::PlayerInputs<MAX_PLAYERS_ON_THIS_MACHINE> &inputs, float) {
  nlohmann::json inputEnvelope;
  inputEnvelope["type"] = shared::sim::ClientMessageType::PLAYER_INPUT;

  shared::sim::PlayerInputs<shared::sim::MAX_PLAYERS_PER_CLIENT> inputsToSend{};

  for (std::size_t i{0}; i < inputs.size() && i < inputsToSend.size(); ++i) {
    inputsToSend[i] = inputs[i];
  }

  inputEnvelope["payload"] = inputsToSend;
  client_.send(inputEnvelope);

  if (auto s = state_box_.take()) {
    last_update_time_ = std::chrono::steady_clock::now();
    previous_state_ = std::move(target_state_);
    target_state_ = std::move(*s);
  }
  return interpolateState();
}

const shared::sim::LobbyUpdate OnlineSession::getLobbyUpdate() {
  if (auto u = lobby_update_box_.take())
    lobby_update_ = std::move(*u);
  return lobby_update_;
}

ThisMachinePlayerIds OnlineSession::getPlayersIds() {
  if (auto msg = welcome_message_box_.take())
    welcome_message_ = std::move(*msg);

  ThisMachinePlayerIds ids{};
  for (std::size_t i = 0; i < ids.size() && i < welcome_message_.players_ids.size(); ++i) {
    ids[i] = welcome_message_.players_ids[i];
  }

  return ids;
}

void OnlineSession::sendReady(shared::sim::PlayerId playerId, bool isReady) {
  nlohmann::json readyEnvelope;
  readyEnvelope["type"] = shared::sim::ClientMessageType::READY;
  readyEnvelope["payload"] = shared::sim::ReadyMessage{.player_id = playerId, .is_ready = isReady};

  client_.send(readyEnvelope);
}

void OnlineSession::sendSwitchTeam(shared::sim::PlayerId playerId, shared::sim::TeamId teamId) {
  nlohmann::json switchEnvelope;
  switchEnvelope["type"] = shared::sim::ClientMessageType::SWITCH_TEAM;
  switchEnvelope["payload"] =
      shared::sim::SwitchTeamMessage{.player_id = playerId, .target_team_id = teamId};

  client_.send(switchEnvelope);
}

void OnlineSession::onMessage(const nlohmann::json &j) {
  try {
    auto payload = j.at("payload");

    switch (j.at("type").get<shared::sim::ServerMessageType>()) {
    case shared::sim::ServerMessageType::COOP_GAME_LOBBY_UPDATE:
      lobby_update_box_.set(payload.get<shared::sim::CoopGameLobbyUpdate>());
      break;
    case shared::sim::ServerMessageType::PVP_GAME_LOBBY_UPDATE:
      lobby_update_box_.set(payload.get<shared::sim::PvPGameLobbyUpdate>());
      break;
    case shared::sim::ServerMessageType::COOP_GAME_STATE:
      state_box_.set(payload.get<shared::sim::CoopGameState>());
      break;
    case shared::sim::ServerMessageType::PVP_GAME_STATE:
      state_box_.set(payload.get<shared::sim::PvPGameState>());
      break;
    case shared::sim::ServerMessageType::WELCOME:
      welcome_message_box_.set(payload.get<shared::sim::WelcomeMessage>());
      break;
    }
  } catch (const nlohmann::json::exception &e) {
    return;
  }
}

shared::sim::GameState OnlineSession::interpolateState() const {
  if (!previous_state_.has_value())
    return target_state_;

  shared::sim::GameState interpolatedState = target_state_;
  float alpha = std::clamp(
      std::chrono::duration<float>(std::chrono::steady_clock::now() - last_update_time_).count() /
          shared::sim::FIXED_DT,
      0.0f,
      1.0f
  );

  auto findIt = [](auto &container, auto id) {
    auto it = std::find_if(container.begin(), container.end(), [&id](const auto &p) {
      return p && p->id == id;
    });

    return std::pair{it != container.end(), it};
  };

  std::visit(
      shared::overloaded{
          [&alpha, &findIt](
              const shared::sim::CoopGameState &prevS,
              const shared::sim::CoopGameState &targetS,
              shared::sim::CoopGameState &interS
          ) {
            for (const auto &targetP : targetS.players) {
              if (targetP) {
                auto [res, previousPlayerIt] = findIt(prevS.players, targetP->id);

                if (res) {
                  auto [_, interPlayerIt] = findIt(interS.players, targetP->id);

                  (*interPlayerIt)->position =
                      (*previousPlayerIt)->position.lerpedWith(targetP->position, alpha);
                }
              }
            }

            interS.boss.position = prevS.boss.position.lerpedWith(targetS.boss.position, alpha);
            interS.enemies_offset = prevS.enemies_offset.lerpedWith(targetS.enemies_offset, alpha);
          },
          [&alpha, &findIt](
              const shared::sim::PvPGameState &prevS,
              const shared::sim::PvPGameState &targetS,
              shared::sim::PvPGameState &interS
          ) {
            for (const auto &targetTeam : targetS.teams) {
              if (targetTeam) {
                auto [res, prevTeamIt] = findIt(prevS.teams, targetTeam->id);

                if (!res)
                  continue;

                auto [_, interTeamIt] = findIt(interS.teams, targetTeam->id);

                for (const auto &targetPlayer : targetTeam->players) {
                  auto [res, prevPlayerIt] = findIt((*prevTeamIt)->players, targetPlayer->id);

                  if (res) {
                    auto [_, interPlayerIt] = findIt((*interTeamIt)->players, targetPlayer->id);

                    (*interPlayerIt)->position =
                        (*prevPlayerIt)->position.lerpedWith(targetPlayer->position, alpha);
                  }
                }
              }
            }
          },
          [](auto &, auto &, auto &) {}
      },
      previous_state_.value(),
      target_state_,
      interpolatedState
  );

  auto lerpBullets = [](const shared::sim::CommonGameState &prevS,
                        const shared::sim::CommonGameState &targetS,
                        shared::sim::CommonGameState &interS,
                        float alpha) {
    for (std::size_t idx = 0; idx < interS.bullets.size(); ++idx) {
      auto &targetBullet = targetS.bullets[idx];
      auto &previousBullet = prevS.bullets[idx];

      if (targetBullet.active && previousBullet.active)
        interS.bullets[idx].position =
            previousBullet.position.lerpedWith(targetBullet.position, alpha);
    }
  };

  std::visit(
      shared::overloaded{
          [&alpha, &lerpBullets](
              const shared::sim::CoopGameState &prevS,
              const shared::sim::CoopGameState &targetS,
              shared::sim::CoopGameState &interS
          ) { lerpBullets(prevS, targetS, interS, alpha); },
          [&alpha, &lerpBullets](
              const shared::sim::PvPGameState &prevS,
              const shared::sim::PvPGameState &targetS,
              shared::sim::PvPGameState &interS
          ) { lerpBullets(prevS, targetS, interS, alpha); },
          [](auto &, auto &, auto &) {}
      },
      previous_state_.value(),
      target_state_,
      interpolatedState
  );

  return interpolatedState;
}

shared::sim::PlayerCount OnlineSession::getPlayerCount() const {
  return player_count_;
}

std::optional<LobbyPlayer>
findLobbyPlayer(const shared::sim::LobbyUpdate &lobby, shared::sim::PlayerId id) {
  const auto inSlots = [id](const auto &slots) -> std::optional<shared::sim::PlayerInfo> {
    for (const auto &p : slots)
      if (p && p->id == id)
        return p;
    return std::nullopt;
  };

  return std::visit(
      shared::overloaded{
          [&](const shared::sim::CoopGameLobbyUpdate &u) -> std::optional<LobbyPlayer> {
            if (auto p = inSlots(u.players))
              return std::pair{*p, std::size_t{0}};
            return std::nullopt;
          },
          [&](const shared::sim::PvPGameLobbyUpdate &u) -> std::optional<LobbyPlayer> {
            for (std::size_t t = 0; t < u.teams.size(); ++t) {
              if (!u.teams[t])
                continue;
              if (auto p = inSlots(u.teams[t]->players))
                return std::pair{*p, t};
            }
            return std::nullopt;
          }
      },
      lobby
  );
}
