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
#include "shared/messages.h"
#include <optional>

namespace testing {

/// Steps @p game at the fixed tick rate until @p predicate holds, up to
/// @p maxTicks. Returns whether it held.
///
/// Always ticks at least once, and checks the predicate only after a tick -
/// predicates here read the broadcast state, which does not exist until the
/// first `update()`. The budget is generous because the entrance phases are
/// time-based (several seconds of sim time), and bounded so a
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

/// One player from a broadcast coop state, looked up by sim id within the
/// live `player_count` prefix. Entries past that count are unspecified
/// padding on the wire (the dense count+array boundary L68 kept on purpose),
/// so a lookup must not consider them.
inline std::optional<shared::PlayerState>
findPlayer(const shared::CoopGameState &state, shared::PlayerId id) {
  for (std::size_t i = 0; i < state.player_count && i < state.players.size();
       ++i) {
    if (state.players[i].id == id)
      return state.players[i];
  }

  return std::nullopt;
}

/// Same for PvP, searching every team's live `size` prefix.
inline std::optional<shared::PlayerState>
findPlayer(const shared::PvPGameState &state, shared::PlayerId id) {
  for (std::size_t teamIdx = 0;
       teamIdx < state.teams_count && teamIdx < state.teams.size(); ++teamIdx) {
    const auto &team = state.teams[teamIdx];

    for (std::size_t i = 0; i < team.size && i < team.players.size(); ++i) {
      if (team.players[i].id == id)
        return team.players[i];
    }
  }

  return std::nullopt;
}

} // namespace testing
