/// Behavioural tests for `PvPGameSim`, driven only through its public
/// surface. PvP has no enemies or boss and therefore no `RndGenerator` use at
/// all, so every case here is fully deterministic.

#include "test_harness.h"

#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/sim/player_sim.h"

#include <array>

using namespace shared;

namespace {

using Phase = PvPGameSim::Phase;
using PvPInputs = std::array<PlayerInput, MAX_TEAMS * MAX_PLAYERS_PER_TEAM>;

Phase phaseOf(const PvPGameState &state) {
  return static_cast<Phase>(state.phase);
}

bool stepUntil(PvPGameSim &sim, PvPGameState &state, const PvPInputs &inputs,
               int maxTicks, bool (*pred)(const PvPGameState &)) {
  for (int tick = 0; tick < maxTicks; ++tick) {
    sim.step(state, inputs, FIXED_DT);
    if (pred(state))
      return true;
  }

  return false;
}

bool reachedFight(const PvPGameState &state) {
  return static_cast<Phase>(state.phase) == Phase::PLAYERS_FIGHT;
}

TEST(pvp_init_rejects_invalid_team_shapes) {
  PvPGameSim sim;

  CHECK_THROWS(sim.init(0, 2));
  CHECK_THROWS(sim.init(MAX_PLAYERS_PER_TEAM + 1, 2));
  CHECK_THROWS(sim.init(1, 1));
  CHECK_THROWS(sim.init(1, MAX_TEAMS + 1));
}

TEST(pvp_start_assigns_unique_ids_across_teams) {
  PvPGameSim sim;
  sim.init(MAX_PLAYERS_PER_TEAM, MAX_TEAMS);

  const auto ids = sim.start();

  CHECK_EQ(ids[0][0], 1u);
  CHECK_EQ(ids[0][1], 2u);
  CHECK_EQ(ids[1][0], 3u);
  CHECK_EQ(ids[1][1], 4u);
}

/// Regression: `Team::size` was never assigned, so it reached the wire as 0
/// and every consumer bounded by it - entrance stepping, input routing,
/// client rendering - silently iterated nothing.
TEST(pvp_state_reports_team_shape) {
  PvPGameSim sim;
  sim.init(2, 2);
  sim.start();

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(state.teams_count, 2);
  CHECK_EQ(state.team_size, 2);
  CHECK_EQ(state.teams[0].size, 2);
  CHECK_EQ(state.teams[1].size, 2);
  CHECK_EQ(state.teams[0].id, 1);
  CHECK_EQ(state.teams[1].id, 2);
}

/// Regression: the entrance phase computed `allEntrancesCompleted` and then
/// discarded it, so the match never left `PLAYERS_ENTRANCE`.
TEST(pvp_entrance_phase_terminates_and_hands_over_to_fight) {
  PvPGameSim sim;
  sim.init(1, 2);
  sim.start();

  PvPGameState state{};
  const PvPInputs idle{};

  sim.step(state, idle, FIXED_DT);
  CHECK_EQ(phaseOf(state), Phase::PLAYERS_ENTRANCE);

  // Both teams slide 80px at 50px/s, so ~96 ticks.
  CHECK(stepUntil(sim, state, idle, 1000, reachedFight));
}

TEST(pvp_teams_enter_from_opposite_ends_facing_each_other) {
  PvPGameSim sim;
  sim.init(1, 2);
  sim.start();

  PvPGameState state{};
  const PvPInputs idle{};
  REQUIRE(stepUntil(sim, state, idle, 1000, reachedFight));

  const auto &top = state.teams[0].players[0];
  const auto &bottom = state.teams[1].players[0];

  CHECK_NEAR(top.position.y, SCREEN_HEIGHT - PlayerSimState::POSITION_Y, 0.5);
  CHECK_NEAR(bottom.position.y, PlayerSimState::POSITION_Y, 0.5);

  // Single-player teams are centred, so the two are aligned on x - which is
  // what makes the duel test below deterministic.
  CHECK_NEAR(top.position.x, bottom.position.x, 0.001);
}

/// Covers input routing end to end: an input is matched to a player by id,
/// never by slot, so an input addressed to one player must leave every other
/// player untouched.
TEST(pvp_movement_input_is_routed_by_player_id) {
  PvPGameSim sim;
  sim.init(1, 2);
  const auto ids = sim.start();

  PvPGameState state{};
  const PvPInputs idle{};
  REQUIRE(stepUntil(sim, state, idle, 1000, reachedFight));

  const float startX = state.teams[1].players[0].position.x;

  PvPInputs inputs{};
  inputs[0] = {.buttons = BUTTON_RIGHT, .player_id = ids[1][0]};

  for (int tick = 0; tick < 30; ++tick)
    sim.step(state, inputs, FIXED_DT);

  CHECK(state.teams[1].players[0].position.x > startX);
  // The unaddressed player must not have moved.
  CHECK_NEAR(state.teams[0].players[0].position.x, startX, 0.001);
}

/// Covers bullet ownership, bullet stepping and `checkCollisions` together:
/// two aligned opponents firing at each other must both take damage and both
/// be credited points.
TEST(pvp_opposing_fire_costs_lives_and_awards_points) {
  PvPGameSim sim;
  sim.init(1, 2);
  const auto ids = sim.start();

  PvPGameState state{};
  const PvPInputs idle{};
  REQUIRE(stepUntil(sim, state, idle, 1000, reachedFight));

  REQUIRE(state.teams[0].players[0].lives == PvPGameSim::INITIAL_LIVES);
  REQUIRE(state.teams[1].players[0].lives == PvPGameSim::INITIAL_LIVES);

  PvPInputs firing{};
  firing[0] = {.buttons = BUTTON_SHOOT, .player_id = ids[0][0]};
  firing[1] = {.buttons = BUTTON_SHOOT, .player_id = ids[1][0]};

  // 660px of travel at 600px/s is ~66 ticks, and the fire cooldown is 9
  // ticks, so 100 ticks lands a handful of hits on each side without either
  // player running out of its 10 lives.
  for (int tick = 0; tick < 100; ++tick)
    sim.step(state, firing, FIXED_DT);

  const auto &top = state.teams[0].players[0];
  const auto &bottom = state.teams[1].players[0];

  CHECK(top.lives < PvPGameSim::INITIAL_LIVES);
  CHECK(bottom.lives < PvPGameSim::INITIAL_LIVES);
  CHECK(top.points >= static_cast<uint32_t>(PvPGameSim::POINTS_PER_HIT));
  CHECK(bottom.points >= static_cast<uint32_t>(PvPGameSim::POINTS_PER_HIT));

  // Neither should be dead yet, so the match is still live.
  CHECK(top.lives > 0);
  CHECK(bottom.lives > 0);
  CHECK_EQ(phaseOf(state), Phase::PLAYERS_FIGHT);
}

TEST(pvp_firing_puts_bullets_on_the_wire) {
  PvPGameSim sim;
  sim.init(1, 2);
  const auto ids = sim.start();

  PvPGameState state{};
  const PvPInputs idle{};
  REQUIRE(stepUntil(sim, state, idle, 1000, reachedFight));

  PvPInputs firing{};
  firing[0] = {.buttons = BUTTON_SHOOT, .player_id = ids[1][0]};

  for (int tick = 0; tick < 20; ++tick)
    sim.step(state, firing, FIXED_DT);

  std::size_t activeBullets = 0;
  for (const auto &bullet : state.bullets) {
    if (bullet.active)
      ++activeBullets;
  }

  CHECK(activeBullets > 0);
}

} // namespace
