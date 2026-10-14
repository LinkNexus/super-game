#pragma once

/// Helpers for driving a `Game` through real ticks and reading what it put on
/// the wire.
///
/// Assertions go through the broadcast payload rather than through getters,
/// deliberately: what a client actually receives is the server's contract, and
/// a test that reads private state through a friend accessor would pass even
/// if the state never reached anybody.

#include "fake_connection.h"
#include "game.h"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include <cstddef>
#include <optional>
#include <string_view>

namespace testing {

/// Steps @p game at the fixed tick rate until @p predicate holds, up to
/// @p maxTicks. Returns whether it held.
///
/// Always ticks at least once, and checks the predicate only after a tick -
/// predicates here read the broadcast state, which does not exist until the
/// first `update()`. The budget is generous because the coop entrance phase
/// is time-based (several seconds of sim time), and bounded so a
/// never-satisfied condition fails the case instead of hanging the suite.
template <typename Predicate>
bool stepUntil(Game &game, Predicate predicate, int maxTicks = 1200) {
  for (int tick = 0; tick < maxTicks; ++tick) {
    game.update(shared::FIXED_DT);

    if (predicate())
      return true;
  }

  return false;
}

/// The coop state carried by @p connection's most recent frame.
inline shared::CoopGameState coopState(const FakeConnection &connection) {
  return connection.lastEnvelope().at("payload").get<shared::CoopGameState>();
}

inline shared::PvPGameState pvpState(const FakeConnection &connection) {
  return connection.lastEnvelope().at("payload").get<shared::PvPGameState>();
}

/// One player from a broadcast coop state, looked up by id.
///
/// Vacancy is the `std::optional` itself now - there is no count field on the
/// wire to bound a scan by, and no implicit id-0 sentinel to skip.
inline std::optional<shared::PlayerState>
findPlayer(const shared::CoopGameState &state, shared::PlayerId id) {
  for (const auto &player : state.players) {
    if (player && player->id == id)
      return player;
  }

  return std::nullopt;
}

/// Same for PvP, searching every engaged team slot.
inline std::optional<shared::PlayerState>
findPlayer(const shared::PvPGameState &state, shared::PlayerId id) {
  for (const auto &team : state.teams) {
    if (!team)
      continue;

    for (const auto &player : team->players) {
      if (player && player->id == id)
        return player;
    }
  }

  return std::nullopt;
}

/// How many player slots the broadcast actually carries.
inline std::size_t livePlayers(const shared::CoopGameState &state) {
  return shared::count_optional(state.players);
}

inline std::size_t livePlayers(const shared::PvPGameState &state) {
  std::size_t found = 0;

  for (const auto &team : state.teams) {
    if (team)
      found += shared::count_optional(team->players);
  }

  return found;
}

/// The team carrying @p id, or nullptr. Teams are looked up by id rather than
/// by slot for the same reason players are.
inline const shared::TeamState *teamById(const shared::PvPGameState &state,
                                         shared::PvPGameSim::TeamId id) {
  for (const auto &team : state.teams) {
    if (team && team->id == id)
      return &team.value();
  }

  return nullptr;
}

/// Whether any player in @p state carries @p name.
///
/// Name-based rather than id-based on purpose: the name-patching cases must
/// keep holding across the id-namespace collapse, which changed *which* ids
/// show up here. A missing patch still leaves the name empty, so this keeps
/// the assertion's teeth either way.
inline bool hasPlayerNamed(const shared::CoopGameState &state,
                           std::string_view name) {
  for (const auto &player : state.players) {
    if (player && player->name == name)
      return true;
  }

  return false;
}

inline bool hasPlayerNamed(const shared::PvPGameState &state,
                           std::string_view name) {
  for (const auto &team : state.teams) {
    if (!team)
      continue;

    for (const auto &player : team->players) {
      if (player && player->name == name)
        return true;
    }
  }

  return false;
}

} // namespace testing
