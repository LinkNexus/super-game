/// Behavioural tests for `CoopGameSim`. Everything here is driven only
/// through the public surface - `start`/`step`/`removePlayer` and the
/// `CoopGameState` written out each tick - so the tests stay valid across
/// refactors of the sim's internals.
///
/// These cases deliberately avoid depending on `RndGenerator`: enemy shot
/// timing is the only randomised input to the coop sim, and nothing asserted
/// below is a function of it.

#include "test_harness.h"

#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"

#include <array>
#include <cstddef>

using namespace shared;

namespace {

using Phase = CoopGameSim::Phase;
using CoopInputs = PlayerInputs<MAX_PLAYERS_COOP>;

Phase phaseOf(const CoopGameState &state) {
  return static_cast<Phase>(state.phase);
}

/// How many slots across the *whole* wire array carry @p id.
///
/// Scans every slot rather than a live prefix, deliberately: a vacated slot
/// that still holds the departed player's last snapshot is a ghost ship on
/// the client, and it is exactly what these cases are about.
std::size_t countById(const CoopGameState &state, PlayerId id) {
  std::size_t found = 0;

  for (const auto &player : state.players) {
    if (player && player->id == id)
      ++found;
  }

  return found;
}

std::size_t livePlayers(const CoopGameState &state) {
  return count_optional(state.players);
}

std::size_t aliveEnemies(const CoopGameState &state) {
  std::size_t found = 0;

  for (const auto &enemy : state.enemies) {
    if (enemy[0] == 1)
      ++found;
  }

  return found;
}

/// Steps until @p pred holds, or gives up after @p maxTicks.
/// @return true if the predicate was satisfied.
bool stepUntil(CoopGameSim &sim, CoopGameState &state, const CoopInputs &inputs,
               int maxTicks, bool (*pred)(const CoopGameState &)) {
  for (int tick = 0; tick < maxTicks; ++tick) {
    sim.step(state, inputs, FIXED_DT);
    if (pred(state))
      return true;
  }

  return false;
}

/// The upper bound on the roster is a `requires` clause on `start`, so it is
/// a compile error rather than a runtime one and cannot be tested here. The
/// empty roster is the only reachable rejection.
TEST(coop_start_rejects_an_empty_roster) {
  CoopGameSim sim;

  CHECK_THROWS(sim.start(PlayerIds<MAX_PLAYERS_COOP>{}));
}

TEST(coop_start_uses_the_ids_it_is_given) {
  CoopGameSim sim;
  sim.start(PlayerIds<3>{7u, 8u, 9u});

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(livePlayers(state), 3u);
  CHECK_EQ(countById(state, 7), 1u);
  CHECK_EQ(countById(state, 8), 1u);
  CHECK_EQ(countById(state, 9), 1u);
}

TEST(coop_state_mirrors_roster_after_first_step) {
  CoopGameSim sim;
  sim.start(PlayerIds<2>{1u, 2u});

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  REQUIRE(livePlayers(state) == 2u);

  for (const auto &player : state.players) {
    if (player) {
      CHECK_EQ(player->lives, PlayerSimState::INITIAL_LIVES);
      CHECK_EQ(player->points, 0u);
    }
  }
}

TEST(coop_entrance_phase_terminates_and_hands_over_to_fight) {
  CoopGameSim sim;
  sim.start(PlayerIds<MAX_PLAYERS_COOP>{1u, 2u, 3u, 4u});

  CoopGameState state{};
  const CoopInputs idle{};

  // The grid starts off-screen; one tick must not be enough to finish.
  sim.step(state, idle, FIXED_DT);
  CHECK_EQ(phaseOf(state), Phase::ENEMIES_ENTRANCE);

  // A full-size grid descends ~500px at 70px/s, so ~430 ticks. The bound is
  // deliberately generous - the assertion is "it terminates at all", which is
  // the regression that matters.
  const bool reachedFight =
      stepUntil(sim, state, idle, 3000, [](const CoopGameState &s) {
        return static_cast<Phase>(s.phase) == Phase::FIGHT_ENEMIES;
      });

  CHECK(reachedFight);
}

TEST(coop_entrance_leaves_enemies_alive_and_boss_inactive) {
  CoopGameSim sim;
  sim.start(PlayerIds<1>{1u});

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  std::size_t aliveEnemies = 0;
  for (const auto &enemy : state.enemies) {
    if (enemy[0] == 1)
      ++aliveEnemies;
  }

  CHECK(aliveEnemies > 0);
  CHECK(!state.boss.active);
}

// --- Removal leaves nothing behind (L63) -----------------------------------
//
// These replace an earlier `coop_remove_player_compacts_the_roster`, which
// asserted the behaviour L63 exists to remove.
//
// They no longer assert that a surviving player's *slot index* is stable.
// That was the right requirement while the client lerped players by array
// index between snapshots; now that `OnlineSession::interpolateState` matches
// players by id, where a player sits in the wire array is unobservable and
// asserting it would test the layout instead of the requirement.
//
// What still matters, and what these pin: a departed player must leave
// nothing addressable behind. The client iterates the array and draws every
// engaged slot, so a stale or duplicated entry is a ghost ship - or two ships
// for one player. No assertions on any count field either; `player_count` is
// gone from the wire and liveness is now the optional itself.

TEST(coop_remove_player_leaves_no_duplicate_or_stale_entry) {
  CoopGameSim sim;
  sim.start(PlayerIds<3>{1u, 2u, 3u});

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  REQUIRE(countById(state, 1) == 1u);

  // The player in the first slot leaves - the case that shifts the most.
  sim.removePlayer(1);
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(countById(state, 1), 0u);
  CHECK_EQ(countById(state, 2), 1u);
  CHECK_EQ(countById(state, 3), 1u);
  CHECK_EQ(livePlayers(state), 2u);
}

/// Two removals in a row, from both ends, so the fix can't work for a single
/// hole and break for two.
TEST(coop_two_removals_leave_only_the_remaining_player) {
  CoopGameSim sim;
  sim.start(PlayerIds<3>{1u, 2u, 3u});

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  sim.removePlayer(1);
  sim.step(state, idle, FIXED_DT);
  sim.removePlayer(3);
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(countById(state, 1), 0u);
  CHECK_EQ(countById(state, 3), 0u);
  CHECK_EQ(countById(state, 2), 1u);
  CHECK_EQ(livePlayers(state), 1u);
}

TEST(coop_losing_the_last_player_ends_the_match) {
  CoopGameSim sim;
  sim.start(PlayerIds<2>{1u, 2u});

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  sim.removePlayer(1);
  sim.removePlayer(2);
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(livePlayers(state), 0u);
  CHECK_EQ(phaseOf(state), Phase::GAME_OVER);
}

TEST(coop_removing_an_unknown_id_is_a_no_op) {
  CoopGameSim sim;
  sim.start(PlayerIds<2>{1u, 2u});

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  sim.removePlayer(9999);
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(livePlayers(state), 2u);
  CHECK_EQ(phaseOf(state), Phase::ENEMIES_ENTRANCE);
}

// --- Difficulty is set at start, not by live occupancy ---------------------
//
// `initial_player_count_` is the *match size*, captured once in `start()`.
// The live roster shrinks on disconnect; the difficulty must not follow it,
// or a 4-player match quietly turns into a 1-player one the moment somebody
// leaves - and the two halves would disagree, since the enemy grid is sized
// in `start()` while the boss is sized later, at the FIGHT_ENEMIES ->
// BOSS_ENTRANCE transition.

TEST(coop_enemy_grid_scales_with_the_starting_player_count) {
  CoopGameSim solo;
  solo.start(PlayerIds<1>{1u});
  CoopGameState soloState{};
  const CoopInputs idle{};
  solo.step(soloState, idle, FIXED_DT);

  CoopGameSim full;
  full.start(PlayerIds<MAX_PLAYERS_COOP>{1u, 2u, 3u, 4u});
  CoopGameState fullState{};
  full.step(fullState, idle, FIXED_DT);

  CHECK(aliveEnemies(soloState) > 0u);
  CHECK(aliveEnemies(fullState) > aliveEnemies(soloState));
  // A full roster gets the whole grid - the top row of ROWS_PER_PLAYERS_COUNT.
  CHECK_EQ(aliveEnemies(fullState),
           static_cast<std::size_t>(EnemiesPoolSimState::MAX_ROWS *
                                    EnemiesPoolSimState::COLS));
}

/// The regression this pins: if the grid were sized from live occupancy
/// rather than the stored match size, losing three of four players would
/// shrink it mid-match.
TEST(coop_enemy_grid_does_not_shrink_when_players_leave) {
  CoopGameSim sim;
  sim.start(PlayerIds<MAX_PLAYERS_COOP>{1u, 2u, 3u, 4u});

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);
  const auto before = aliveEnemies(state);
  REQUIRE(before > 0u);

  sim.removePlayer(1);
  sim.removePlayer(2);
  sim.removePlayer(3);
  sim.step(state, idle, FIXED_DT);

  REQUIRE(livePlayers(state) == 1u);
  // Idle inputs kill nothing, so any change here is a re-sizing, not a fight.
  CHECK_EQ(aliveEnemies(state), before);
}

/// The boss half of the same invariant, tested on `BossSimState` directly.
///
/// Reaching BOSS_ENTRANCE through `CoopGameSim` means clearing all 80
/// enemies, which needs a long fight whose outcome depends on `RndGenerator`
/// (enemy return fire) - exactly what the rest of this file avoids. So this
/// covers the scaling table itself; that `step()` feeds it the *stored*
/// count rather than the live one is covered by the grid cases above, which
/// read the same field.
TEST(boss_difficulty_scales_with_the_count_it_is_given) {
  BossSimState solo{};
  BossSimState full{};
  solo.init(1);
  full.init(MAX_PLAYERS_COOP);

  CHECK_EQ(solo.max_health, BossSimState::INITIAL_LP);
  CHECK_EQ(full.max_health, BossSimState::INITIAL_LP * MAX_PLAYERS_COOP);
  CHECK_EQ(full.health, full.max_health);

  CHECK(full.spread_shot_bullets_count > solo.spread_shot_bullets_count);
  CHECK(full.successive_shots_bullets_count >
        solo.successive_shots_bullets_count);

  // A full roster gets the top of both tables.
  CHECK_EQ(full.spread_shot_bullets_count,
           BossSimState::MAX_SPREAD_SHOT_BULLETS);
  CHECK_EQ(full.successive_shots_bullets_count,
           BossSimState::MAX_SUCCESSIVE_SHOTS_BULLETS);
}

} // namespace
