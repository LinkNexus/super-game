/// Roster, matchmaking and broadcast cases for `supergame-server-lib`.

#include "fake_connection.h"
#include "game.h"
#include "game_manager.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "test_harness.h"

using testing::FakeConnection;
using testing::fakeConnection;

TEST(coop_two_single_slot_connections_occupy_distinct_slots) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  CHECK_EQ(shared::count_optional(game.getPlayers()), 2);
  CHECK(game.findPlayerById(1) != nullptr);
  CHECK(game.findPlayerById(2) != nullptr);
  // Nobody has readied, so the match must still be waiting.
  CHECK_EQ(game.isRunning(), false);
}

TEST(coop_one_connection_can_claim_two_slots) {
  FakeConnection a;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1, 2})));

  CHECK_EQ(shared::count_optional(game.getPlayers()), 2);
  CHECK_EQ(game.getPlayerSockets().size(), 1u);
  // One connection is not "enough players" for online coop, whatever the slot
  // count - see the L58/L59 decision in ROADMAP.md Phase 4.
  CHECK_EQ(game.thereIsEnoughPlayers(), false);
}

/// The L59 regression: once a mid-lobby disconnect leaves a hole, the player
/// count is no longer the first free index. Writing at the count overwrote a
/// still-connected player, who then couldn't move and whose socket was never
/// erased.
TEST(coop_join_after_mid_lobby_disconnect_keeps_the_later_player) {
  FakeConnection a, b, c, d;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));
  REQUIRE(game.addPlayers(fakeConnection(c, {3})));

  game.removePlayers(&b);
  REQUIRE(game.findPlayerById(2) == nullptr);

  REQUIRE(game.addPlayers(fakeConnection(d, {4})));

  CHECK_EQ(shared::count_optional(game.getPlayers()), 3);
  CHECK(game.findPlayerById(3) != nullptr);
  CHECK(game.findPlayerById(4) != nullptr);
  CHECK_EQ(game.getPlayerSockets().size(), 3u);
}

TEST(coop_rejects_a_connection_that_would_exceed_the_slot_cap) {
  FakeConnection a, b, c;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1, 2})));
  REQUIRE(game.addPlayers(fakeConnection(b, {3, 4})));
  CHECK_EQ(shared::count_optional(game.getPlayers()),
           shared::MAX_PLAYERS_COOP);

  CHECK_EQ(game.addPlayers(fakeConnection(c, {5})), false);
  CHECK_EQ(shared::count_optional(game.getPlayers()),
           shared::MAX_PLAYERS_COOP);
}

TEST(coop_empties_once_every_connection_leaves) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));
  CHECK_EQ(game.isEmpty(), false);

  game.removePlayers(&a);
  CHECK_EQ(game.isEmpty(), false);

  game.removePlayers(&b);
  CHECK_EQ(game.isEmpty(), true);
  CHECK_EQ(shared::count_optional(game.getPlayers()), 0);
}

TEST(coop_starts_once_two_connections_are_ready) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  game.setPlayersReady(&a, true);
  CHECK_EQ(game.isRunning(), false);

  game.setPlayersReady(&b, true);
  CHECK_EQ(game.isRunning(), true);
}

TEST(pvp_auto_balances_arrivals_across_teams) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  const auto &teams = game.getTeams();
  CHECK_EQ(shared::count_optional(teams[0].players), 1);
  CHECK_EQ(shared::count_optional(teams[1].players), 1);
  CHECK_EQ(game.thereIsEnoughPlayers(), true);
}

TEST(pvp_duo_on_one_connection_lands_on_a_single_team) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  REQUIRE(game.addPlayers(fakeConnection(a, {1, 2})));

  const auto &teams = game.getTeams();
  CHECK_EQ(shared::count_optional(teams[0].players), 2);
  CHECK_EQ(shared::count_optional(teams[1].players), 0);
  CHECK_EQ(game.thereIsEnoughPlayers(), false);
}

TEST(manager_matches_a_second_coop_connection_into_the_same_game) {
  FakeConnection a, b;
  GameManager manager;

  const auto first = manager.joinOrCreateCoopGame(fakeConnection(a, {1}));
  const auto second = manager.joinOrCreateCoopGame(fakeConnection(b, {2}));

  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK_EQ(*first, *second);
  CHECK(manager.findGameById(*first) != nullptr);
}

TEST(manager_keeps_coop_and_pvp_seekers_in_separate_pools) {
  FakeConnection a, b;
  GameManager manager;

  const auto coop = manager.joinOrCreateCoopGame(fakeConnection(a, {1}));
  const auto pvp = manager.joinOrCreatePvPGame(fakeConnection(b, {2}), 1, 2);

  REQUIRE(coop.has_value());
  REQUIRE(pvp.has_value());
  CHECK(*coop != *pvp);
}

TEST(manager_destroys_a_game_and_stops_matching_into_it) {
  FakeConnection a, b;
  GameManager manager;

  const auto id = manager.joinOrCreateCoopGame(fakeConnection(a, {1}));
  REQUIRE(id.has_value());

  manager.destroyGame(*id);
  CHECK(manager.findGameById(*id) == nullptr);

  // The destroyed game must also be gone from the open-games pool, or the
  // next joiner gets matched into freed memory.
  const auto next = manager.joinOrCreateCoopGame(fakeConnection(b, {2}));
  REQUIRE(next.has_value());
  CHECK(*next != *id);
}
