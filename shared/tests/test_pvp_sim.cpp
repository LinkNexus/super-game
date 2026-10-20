/// Behavioural tests for `PvPGameSim`, driven only through its public
/// surface. PvP has no enemies or boss and therefore no `RndGenerator` use at
/// all, so every case here is fully deterministic.

#include "test_harness.h"

#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include "shared/sim/player_sim.h"

#include <array>
#include <cstddef>
#include <utility>

using namespace shared;

namespace {

using Phase = PvPGameSim::Phase;
using Outcome = PvPGameSim::TeamOutcome;
using TeamId = PvPGameSim::TeamId;
using PvPInputs = PlayerInputs<MAX_TEAMS * MAX_PLAYERS_PER_TEAM>;
using TeamRoster = std::pair<TeamId, PlayerIds<MAX_PLAYERS_PER_TEAM>>;

Phase phaseOf(const PvPGameState &state) {
  return static_cast<Phase>(state.phase);
}

Outcome outcomeOf(const TeamState &team) {
  return static_cast<Outcome>(team.outcome);
}

/// Two teams, each roster given as the ids on that team. Team ids are 1-based
/// because the sim indexes an internal status array by `id - 1`.
PvPGameSim::TeamRosters
rosters(const PlayerIds<MAX_PLAYERS_PER_TEAM> &first,
        const PlayerIds<MAX_PLAYERS_PER_TEAM> &second) {
  PvPGameSim::TeamRosters ids{};
  ids[0] = TeamRoster{1, first};
  ids[1] = TeamRoster{2, second};

  return ids;
}

/// The team carrying @p id, searched across every engaged team slot.
const TeamState *teamById(const PvPGameState &state, TeamId id) {
  for (const auto &team : state.teams) {
    if (team && team->id == id)
      return &team.value();
  }

  return nullptr;
}

/// How many slots across @p state's whole team/player arrays carry @p id - a
/// vacated slot still holding a departed player is a ghost ship on the
/// client, which is what the removal cases below are about.
std::size_t countById(const PvPGameState &state, PlayerId id) {
  std::size_t found = 0;

  for (const auto &team : state.teams) {
    if (!team)
      continue;

    for (const auto &player : team->players) {
      if (player && player->id == id)
        ++found;
    }
  }

  return found;
}

const PlayerState *findPlayer(const PvPGameState &state, PlayerId id) {
  for (const auto &team : state.teams) {
    if (!team)
      continue;

    for (const auto &player : team->players) {
      if (player && player->id == id)
        return &player.value();
    }
  }

  return nullptr;
}

TEST(pvp_start_rejects_a_team_with_no_players) {
  PvPGameSim sim;

  CHECK_THROWS(sim.start(rosters({5u, 6u}, {})));
}

/// The ids handed to `start` must land on their own team, one player per id.
/// Both halves matter: a team whose two players share an id is one ship the
/// sim can never address separately, and input routing is by id.
TEST(pvp_start_assigns_each_given_id_to_its_team) {
  PvPGameSim sim;
  sim.start(rosters({5u, 6u}, {7u, 8u}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(countById(state, 5), 1u);
  CHECK_EQ(countById(state, 6), 1u);
  CHECK_EQ(countById(state, 7), 1u);
  CHECK_EQ(countById(state, 8), 1u);

  const auto *first = teamById(state, 1);
  const auto *second = teamById(state, 2);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK_EQ(count_optional(first->players), 2u);
  CHECK_EQ(count_optional(second->players), 2u);
}

TEST(pvp_teams_start_at_opposite_ends_facing_each_other) {
  PvPGameSim sim;
  sim.start(rosters({1u, std::nullopt}, {2u, std::nullopt}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  const auto *top = findPlayer(state, 1);
  const auto *bottom = findPlayer(state, 2);
  REQUIRE(top != nullptr);
  REQUIRE(bottom != nullptr);

  CHECK_NEAR(top->position.y, SCREEN_HEIGHT - PlayerSimState::POSITION_Y, 0.5);
  CHECK_NEAR(bottom->position.y, PlayerSimState::POSITION_Y, 0.5);

  // Facing each other: the two orientations must differ, or the top team's
  // bullets would travel away from their opponents.
  CHECK(top->orientation != bottom->orientation);

  // Single-player teams are centred, so the two are aligned on x - which is
  // what makes the duel case below deterministic.
  CHECK_NEAR(top->position.x, bottom->position.x, 0.001);
}

// --- Two players per team (L81) --------------------------------------------
// Every case above this point gives each team a single player, so nothing in
// the suite has ever asserted what a 2v2 start produces. These two are the
// 2v2 readings of `pvp_teams_start_at_opposite_ends_facing_each_other` and
// `pvp_opposing_fire_costs_lives_and_awards_points` directly above/below, and
// they state what a 2v2 is *supposed* to do rather than what it currently
// does.

/// A team's side of the field is the team's property, not the player's: both
/// of a team's players belong on that team's end, facing the opponents.
TEST(pvp_2v2_puts_each_whole_team_on_its_own_end) {
  PvPGameSim sim;
  sim.start(rosters({1u, 2u}, {3u, 4u}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  const auto *first = findPlayer(state, 1);
  const auto *second = findPlayer(state, 2);
  const auto *third = findPlayer(state, 3);
  const auto *fourth = findPlayer(state, 4);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  REQUIRE(third != nullptr);
  REQUIRE(fourth != nullptr);

  // Teammates share an end and an orientation.
  CHECK_NEAR(first->position.y, second->position.y, 0.001);
  CHECK_NEAR(third->position.y, fourth->position.y, 0.001);
  CHECK_NEAR(first->orientation, second->orientation, 0.001);
  CHECK_NEAR(third->orientation, fourth->orientation, 0.001);

  // The two ends are opposite ones, and the two teams face each other - the
  // same claim the 1v1 case makes, which is orientation-dependent: bullet
  // velocity is rotated by the shooter's orientation, so two teams sharing an
  // orientation can never fire at one another.
  CHECK(first->position.y != third->position.y);
  CHECK(first->orientation != third->orientation);

  // No two players may occupy the same point. Opponents are deliberately
  // aligned on x (that is what makes the combat case below deterministic), so
  // this is a claim about y for cross-team pairs and about x for teammates.
  CHECK(first->position.x != second->position.x);
  CHECK(third->position.x != fourth->position.x);
}

/// The 2v2 reading of the duel's combat case: a 2v2 must be *contestable*.
/// Both teams spawn their players from the same x formula, so each player
/// faces an opponent on its own column - every one of the four should both
/// deal and take damage.
///
/// This is the case that matters most of the two: "the teams spawn apart" is
/// a weaker claim than "the match can be played to a result", and a 2v2 that
/// cannot take a life can only ever end by disconnect.
TEST(pvp_2v2_opposing_fire_costs_lives_on_both_teams) {
  PvPGameSim sim;
  sim.start(rosters({1u, 2u}, {3u, 4u}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  const auto *before = findPlayer(state, 1);
  REQUIRE(before != nullptr);
  const auto initialLives = before->lives;
  REQUIRE(initialLives > 1);

  PvPInputs firing{};
  firing[0] = PlayerInput{.buttons = BUTTON_SHOOT, .player_id = 1};
  firing[1] = PlayerInput{.buttons = BUTTON_SHOOT, .player_id = 2};
  firing[2] = PlayerInput{.buttons = BUTTON_SHOOT, .player_id = 3};
  firing[3] = PlayerInput{.buttons = BUTTON_SHOOT, .player_id = 4};

  // Same budget as the duel case, for the same reasons: ~66 ticks of travel
  // and a 9-tick cooldown, so 100 ticks lands hits without emptying anyone.
  for (int tick = 0; tick < 100; ++tick)
    sim.step(state, firing, FIXED_DT);

  for (const PlayerId id : {1u, 2u, 3u, 4u}) {
    const auto *player = findPlayer(state, id);
    REQUIRE(player != nullptr);

    CHECK(player->lives < initialLives);
    CHECK(player->points > 0u);
    CHECK(player->lives > 0);
  }

  CHECK_EQ(phaseOf(state), Phase::PLAYERS_FIGHT);
}

/// Covers input routing end to end: an input is matched to a player by id,
/// never by slot, so an input addressed to one player must leave every other
/// player untouched.
TEST(pvp_movement_input_is_routed_by_player_id) {
  PvPGameSim sim;
  sim.start(rosters({1u, std::nullopt}, {2u, std::nullopt}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  const auto *before = findPlayer(state, 2);
  REQUIRE(before != nullptr);
  const float startX = before->position.x;

  PvPInputs inputs{};
  inputs[0] = PlayerInput{.buttons = BUTTON_RIGHT, .player_id = 2};

  for (int tick = 0; tick < 30; ++tick)
    sim.step(state, inputs, FIXED_DT);

  const auto *moved = findPlayer(state, 2);
  const auto *still = findPlayer(state, 1);
  REQUIRE(moved != nullptr);
  REQUIRE(still != nullptr);

  CHECK(moved->position.x > startX);
  // The unaddressed player must not have moved.
  CHECK_NEAR(still->position.x, startX, 0.001);
}

/// Covers bullet ownership, bullet stepping and `checkCollisions` together:
/// two aligned opponents firing at each other must both take damage and both
/// be credited points. Initial lives are read off the first broadcast rather
/// than from a constant - `PvPGameSim::INITIAL_LIVES` is private.
TEST(pvp_opposing_fire_costs_lives_and_awards_points) {
  PvPGameSim sim;
  sim.start(rosters({1u, std::nullopt}, {2u, std::nullopt}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  const auto *topBefore = findPlayer(state, 1);
  REQUIRE(topBefore != nullptr);
  const auto initialLives = topBefore->lives;
  REQUIRE(initialLives > 1);

  PvPInputs firing{};
  firing[0] = PlayerInput{.buttons = BUTTON_SHOOT, .player_id = 1};
  firing[1] = PlayerInput{.buttons = BUTTON_SHOOT, .player_id = 2};

  // 660px of travel at 600px/s is ~66 ticks, and the fire cooldown is 9
  // ticks, so 100 ticks lands a handful of hits on each side without either
  // player running out of lives.
  for (int tick = 0; tick < 100; ++tick)
    sim.step(state, firing, FIXED_DT);

  const auto *top = findPlayer(state, 1);
  const auto *bottom = findPlayer(state, 2);
  REQUIRE(top != nullptr);
  REQUIRE(bottom != nullptr);

  CHECK(top->lives < initialLives);
  CHECK(bottom->lives < initialLives);
  CHECK(top->points > 0u);
  CHECK(bottom->points > 0u);

  // Neither should be dead yet, so the match is still live.
  CHECK(top->lives > 0);
  CHECK(bottom->lives > 0);
  CHECK_EQ(phaseOf(state), Phase::PLAYERS_FIGHT);
}

TEST(pvp_firing_puts_bullets_on_the_wire) {
  PvPGameSim sim;
  sim.start(rosters({1u, std::nullopt}, {2u, std::nullopt}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  PvPInputs firing{};
  firing[0] = PlayerInput{.buttons = BUTTON_SHOOT, .player_id = 2};

  for (int tick = 0; tick < 20; ++tick)
    sim.step(state, firing, FIXED_DT);

  std::size_t activeBullets = 0;
  for (const auto &bullet : state.bullets) {
    if (bullet.active)
      ++activeBullets;
  }

  CHECK(activeBullets > 0);
}

// --- Removal leaves nothing behind (L63) -----------------------------------
// See the longer note in test_coop_sim.cpp: slot indices are no longer
// asserted (the client matches players by id), but a departed player must
// leave no stale or duplicated entry on the wire.

TEST(pvp_remove_player_leaves_no_duplicate_or_stale_entry) {
  PvPGameSim sim;
  sim.start(rosters({5u, 6u}, {7u, 8u}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);

  REQUIRE(countById(state, 5) == 1u);

  sim.removePlayer(5);
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(countById(state, 5), 0u);
  CHECK_EQ(countById(state, 6), 1u);
  // The other team must be untouched by a removal on this one.
  CHECK_EQ(countById(state, 7), 1u);
  CHECK_EQ(countById(state, 8), 1u);
}

/// L57's severe case, at sim level: with one team emptied, the match must
/// reach a terminal phase with the survivors credited the win, rather than
/// sitting in PLAYERS_FIGHT forever with no winner possible.
TEST(pvp_last_team_standing_wins_once_the_other_empties) {
  PvPGameSim sim;
  sim.start(rosters({1u, std::nullopt}, {2u, std::nullopt}));

  PvPGameState state{};
  const PvPInputs idle{};
  sim.step(state, idle, FIXED_DT);
  REQUIRE(phaseOf(state) == Phase::PLAYERS_FIGHT);

  sim.removePlayer(1);
  sim.step(state, idle, FIXED_DT);

  CHECK_EQ(phaseOf(state), Phase::END);

  const auto *loser = teamById(state, 1);
  const auto *winner = teamById(state, 2);
  REQUIRE(loser != nullptr);
  REQUIRE(winner != nullptr);
  CHECK_EQ(outcomeOf(*winner), Outcome::WON);
  CHECK_EQ(outcomeOf(*loser), Outcome::LOST);
}

} // namespace
