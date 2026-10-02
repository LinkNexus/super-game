/// Behavioural tests for `CoopGameSim`. Everything here is driven only
/// through the public surface - `init`/`start`/`step`/`removePlayer` and the
/// `CoopGameState` written out each tick - so the tests stay valid across
/// refactors of the sim's internals.
///
/// These cases deliberately avoid depending on `RndGenerator`: enemy shot
/// timing is the only randomised input to the coop sim, and nothing asserted
/// below is a function of it.

#include "test_harness.h"

#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/sim/player_sim.h"

#include <array>

using namespace shared;

namespace {

using Phase = CoopGameSim::Phase;
using CoopInputs = std::array<PlayerInput, MAX_PLAYERS_COOP>;

Phase phaseOf(const CoopGameState &state) {
  return static_cast<Phase>(state.phase);
}

/// Steps until @p pred holds, or gives up after @p maxTicks.
/// @return true if the predicate was satisfied.
bool stepUntil(CoopGameSim &sim, CoopGameState &state,
               const CoopInputs &inputs, int maxTicks,
               bool (*pred)(const CoopGameState &)) {
  for (int tick = 0; tick < maxTicks; ++tick) {
    sim.step(state, inputs, FIXED_DT);
    if (pred(state))
      return true;
  }

  return false;
}

TEST(coop_init_rejects_out_of_range_player_counts) {
  CoopGameSim sim;

  CHECK_THROWS(sim.init(0));
  CHECK_THROWS(sim.init(MAX_PLAYERS_COOP + 1));
}

TEST(coop_start_assigns_sequential_player_ids_from_one) {
  CoopGameSim sim;
  sim.init(3);

  const auto ids = sim.start();

  CHECK_EQ(ids[0], 1u);
  CHECK_EQ(ids[1], 2u);
  CHECK_EQ(ids[2], 3u);
}

TEST(coop_state_mirrors_roster_after_first_step) {
  CoopGameSim sim;
  sim.init(2);
  sim.start();

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(state.player_count, 2);
  CHECK_EQ(state.players[0].id, 1u);
  CHECK_EQ(state.players[1].id, 2u);
  CHECK_EQ(state.players[0].lives, PlayerSimState::INITIAL_LIVES);
  CHECK_EQ(state.players[1].lives, PlayerSimState::INITIAL_LIVES);
  CHECK_EQ(state.players[0].points, 0u);
}

TEST(coop_entrance_phase_terminates_and_hands_over_to_fight) {
  CoopGameSim sim;
  sim.init(MAX_PLAYERS_COOP);
  sim.start();

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
  sim.init(1);
  sim.start();

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

TEST(coop_remove_player_compacts_the_roster) {
  CoopGameSim sim;
  sim.init(3);
  const auto ids = sim.start();

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);
  REQUIRE(state.player_count == 3);

  // Removing from the middle must not leave a hole: the roster is compacted
  // and every loop in the sim is bounded by the count.
  sim.removePlayer(ids[1]);
  sim.step(state, idle, FIXED_DT);
  CHECK_EQ(state.player_count, 2);

  sim.removePlayer(ids[0]);
  sim.step(state, idle, FIXED_DT);
  CHECK_EQ(state.player_count, 1);
}

TEST(coop_losing_the_last_player_ends_the_match) {
  CoopGameSim sim;
  sim.init(2);
  const auto ids = sim.start();

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  sim.removePlayer(ids[0]);
  sim.removePlayer(ids[1]);
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(state.player_count, 0);
  CHECK_EQ(phaseOf(state), Phase::GAME_OVER);
}

TEST(coop_removing_an_unknown_id_is_a_no_op) {
  CoopGameSim sim;
  sim.init(2);
  sim.start();

  CoopGameState state{};
  const CoopInputs idle{};
  sim.step(state, idle, FIXED_DT);

  sim.removePlayer(9999);
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(state.player_count, 2);
  CHECK_EQ(phaseOf(state), Phase::ENEMIES_ENTRANCE);
}

} // namespace
