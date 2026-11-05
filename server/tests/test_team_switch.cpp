/// Lobby team-switch cases (L74): `SWITCH_TEAM` frames driven through
/// `Game::handleClientMessage()`, asserted on the lobby broadcast.
///
/// Wire shape: `{"type": SWITCH_TEAM, "payload": {"player_id": P,
/// "target_team_id": T}}`. Teams are per player: a switch moves only the named
/// player, who must belong to the sender, into a team with a free seat, and
/// clears ready. Team ids come from the lobby (`PvPGameLobbyUpdate.teams[i]
/// ->id`), the same ids `PvPGameState` carries.

#include "fake_connection.h"
#include "game.h"
#include "match_support.h"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/wire.h"
#include "test_harness.h"
#include <cstddef>
#include <optional>
#include <string>
#include <variant>

using testing::FakeConnection;
using testing::fakeConnection;

namespace {

using Result = Game::ClientMessageResult;

std::string switchTeamFrame(shared::PlayerId player_id,
                            long long target_team_id) {
  nlohmann::json envelope{};
  envelope["type"] = shared::ClientMessageType::SWITCH_TEAM;
  envelope["payload"] = {{"player_id", player_id},
                         {"target_team_id", target_team_id}};

  return shared::encode(envelope, shared::WireFormat::JSON);
}

std::string readyFrame(shared::PlayerId player_id, bool is_ready) {
  nlohmann::json envelope{};
  envelope["type"] = shared::ClientMessageType::READY;
  envelope["payload"] = {{"player_id", player_id}, {"is_ready", is_ready}};

  return shared::encode(envelope, shared::WireFormat::JSON);
}

/// Bind the result to a named local (see `coopLobbyFrame` in test_inbound.cpp).
shared::PvPGameLobbyUpdate pvpLobbyFrame(const FakeConnection &connection) {
  return connection.lastEnvelope()
      .at("payload")
      .get<shared::PvPGameLobbyUpdate>();
}

/// The lobby as built right now, without sending anything.
shared::PvPGameLobbyUpdate pvpLobbyOf(const Game &game) {
  return std::get<shared::PvPGameLobbyUpdate>(game.buildLobbyUpdate());
}

/// The id of the team at lobby slot @p slot; fails the case if it's vacant.
shared::TeamId teamIdAt(const shared::PvPGameLobbyUpdate &lobby,
                        std::size_t slot) {
  REQUIRE(lobby.teams[slot].has_value());
  return lobby.teams[slot]->id;
}

/// The id of the team listing player @p id, or nullopt.
std::optional<shared::TeamId> teamOf(const shared::PvPGameLobbyUpdate &lobby,
                                     shared::PlayerId id) {
  for (const auto &team : lobby.teams) {
    if (!team)
      continue;

    for (const auto &player : team->players) {
      if (player && player->id == id)
        return team->id;
    }
  }

  return std::nullopt;
}

std::optional<shared::PlayerInfo>
lobbyPlayer(const shared::PvPGameLobbyUpdate &lobby, shared::PlayerId id) {
  for (const auto &team : lobby.teams) {
    if (!team)
      continue;

    for (const auto &player : team->players) {
      if (player && player->id == id)
        return player;
    }
  }

  return std::nullopt;
}

std::size_t seatsTaken(const shared::PvPGameLobbyUpdate &lobby,
                       shared::TeamId id) {
  for (const auto &team : lobby.teams) {
    if (team && team->id == id)
      return shared::count_optional(team->players);
  }

  return 0;
}

} // namespace

// --- Team ids on the wire ------------------------------------------------

TEST(the_pvp_lobby_gives_every_team_a_distinct_id) {
  FakeConnection a;
  PvPGame game{/*team_size=*/1, /*team_count=*/shared::MAX_TEAMS};

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));

  const auto lobby = pvpLobbyOf(game);
  for (std::size_t i = 0; i < shared::MAX_TEAMS; ++i) {
    for (std::size_t j = i + 1; j < shared::MAX_TEAMS; ++j)
      CHECK(teamIdAt(lobby, i) != teamIdAt(lobby, j));
  }
}

/// What makes the id worth sending: the team a player sees in the lobby is
/// the team the match puts them in.
TEST(a_players_lobby_team_id_is_its_team_id_in_the_match) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));

  const auto lobby = pvpLobbyOf(game);
  const auto lobbyTeamOf1 = teamOf(lobby, 1);
  const auto lobbyTeamOf2 = teamOf(lobby, 2);
  REQUIRE(lobbyTeamOf1.has_value());
  REQUIRE(lobbyTeamOf2.has_value());

  REQUIRE(game.handleClientMessage(aData, readyFrame(1, true)));
  REQUIRE(game.handleClientMessage(bData, readyFrame(2, true)));
  REQUIRE(game.isRunning());
  REQUIRE(testing::stepUntil(game, [] { return true; }));

  const auto state = testing::pvpState(a);
  const auto *team1 = testing::teamById(state, *lobbyTeamOf1);
  const auto *team2 = testing::teamById(state, *lobbyTeamOf2);
  REQUIRE(team1 != nullptr);
  REQUIRE(team2 != nullptr);

  auto lists = [](const shared::TeamState &team, shared::PlayerId id) {
    for (const auto &player : team.players)
      if (player && player->id == id)
        return true;
    return false;
  };
  CHECK(lists(*team1, 1));
  CHECK(lists(*team2, 2));
}

// --- Successful switches -------------------------------------------------

TEST(a_switch_moves_the_named_player_to_a_team_with_a_free_seat) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));

  // Auto-balance fills the first team with room: both start in team 0.
  const auto before = pvpLobbyOf(game);
  const auto t0 = teamIdAt(before, 0);
  const auto t1 = teamIdAt(before, 1);
  REQUIRE(teamOf(before, 1) == std::optional{t0});
  REQUIRE(teamOf(before, 2) == std::optional{t0});

  const auto outcome = game.handleClientMessage(aData, switchTeamFrame(1, t1));
  CHECK_EQ(outcome.result, Result::OK);

  // The other client sees it: the switch is broadcast, not only applied.
  const auto lobby = pvpLobbyFrame(b);
  CHECK(teamOf(lobby, 1) == std::optional{t1});
  CHECK(teamOf(lobby, 2) == std::optional{t0});
  CHECK_EQ(seatsTaken(lobby, t0), 1u);
  CHECK_EQ(seatsTaken(lobby, t1), 1u);
}

/// Per player: one half of a same-machine duo moves, the other stays.
TEST(a_switch_moves_only_the_named_player_of_a_two_slot_connection) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1, 2});
  REQUIRE(game.addPlayers(aData));

  const auto before = pvpLobbyOf(game);
  const auto t0 = teamIdAt(before, 0);
  const auto t1 = teamIdAt(before, 1);

  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(2, t1)));

  const auto lobby = pvpLobbyFrame(a);
  CHECK(teamOf(lobby, 1) == std::optional{t0});
  CHECK(teamOf(lobby, 2) == std::optional{t1});
}

/// Names travel with the player: a switch re-inserts the slot, it must not
/// rebuild it from the id alone.
TEST(a_switched_player_keeps_its_name) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(aData));
  const auto t1 = teamIdAt(pvpLobbyOf(game), 1);

  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t1)));

  const auto lobby = pvpLobbyFrame(a);
  const auto player = lobbyPlayer(lobby, 1);
  REQUIRE(player.has_value());
  CHECK_EQ(player->name, std::string("P1"));
}

/// Switching back and forth must not leak seats (a copy left behind in the
/// old team would list the player twice).
TEST(switching_back_and_forth_leaves_each_player_listed_once) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));

  const auto before = pvpLobbyOf(game);
  const auto t0 = teamIdAt(before, 0);
  const auto t1 = teamIdAt(before, 1);

  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t1)));
  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t0)));
  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t1)));

  const auto lobby = pvpLobbyFrame(a);
  CHECK_EQ(seatsTaken(lobby, t0) + seatsTaken(lobby, t1), 2u);
  CHECK(teamOf(lobby, 1) == std::optional{t1});
  CHECK(teamOf(lobby, 2) == std::optional{t0});
}

/// A duo moves one player at a time: the seat check counts the one player
/// moving, not the whole connection.
TEST(both_players_of_a_two_slot_connection_can_switch_one_after_the_other) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1, 2});
  REQUIRE(game.addPlayers(aData));

  const auto before = pvpLobbyOf(game);
  const auto t0 = teamIdAt(before, 0);
  const auto t1 = teamIdAt(before, 1);

  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t1)));
  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(2, t1)));

  const auto lobby = pvpLobbyFrame(a);
  CHECK(teamOf(lobby, 1) == std::optional{t1});
  CHECK(teamOf(lobby, 2) == std::optional{t1});
  CHECK_EQ(seatsTaken(lobby, t0), 0u);
}

/// Moving into a team that already has a player must list both: the mover
/// needs a free seat of its own in the new team, not its old rank.
TEST(a_switch_into_an_occupied_team_keeps_every_player_listed) {
  FakeConnection a, b, c;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));
  REQUIRE(game.addPlayers(fakeConnection(c, {3})));

  const auto before = pvpLobbyOf(game);
  const auto t0 = teamIdAt(before, 0);
  const auto t1 = teamIdAt(before, 1);
  REQUIRE(teamOf(before, 3) == std::optional{t1});

  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t1)));

  const auto lobby = pvpLobbyFrame(a);
  CHECK(teamOf(lobby, 1) == std::optional{t1});
  CHECK(teamOf(lobby, 2) == std::optional{t0});
  CHECK(teamOf(lobby, 3) == std::optional{t1});
  CHECK_EQ(seatsTaken(lobby, t1), 2u);
}

// --- Rejected switches ---------------------------------------------------

TEST(a_switch_to_a_full_team_is_rejected_and_still_answered) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));

  const auto before = pvpLobbyOf(game);
  const auto t0 = teamIdAt(before, 0);
  const auto t1 = teamIdAt(before, 1);

  const auto sentBefore = a.sent.size();
  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t1)));

  // A rejected switch still answers with the real lobby, so a client that
  // guessed wrong resyncs.
  CHECK(a.sent.size() > sentBefore);

  const auto lobby = pvpLobbyFrame(a);
  CHECK(teamOf(lobby, 1) == std::optional{t0});
  CHECK(teamOf(lobby, 2) == std::optional{t1});
}

/// Same ownership rule as inputs and ready: a connection can only move its
/// own players.
TEST(a_switch_naming_a_player_the_sender_does_not_own_is_ignored) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));

  const auto before = pvpLobbyOf(game);
  const auto t0 = teamIdAt(before, 0);
  const auto t1 = teamIdAt(before, 1);

  game.handleClientMessage(aData, switchTeamFrame(2, t1));

  const auto lobby = pvpLobbyOf(game);
  CHECK(teamOf(lobby, 1) == std::optional{t0});
  CHECK(teamOf(lobby, 2) == std::optional{t0});
}

/// Switching to the team you're already in changes nothing, including the
/// order of the seats inside it and anyone's ready flag.
TEST(a_switch_to_the_players_own_team_is_a_no_op) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));
  REQUIRE(game.handleClientMessage(bData, readyFrame(2, true)));
  const auto t0 = teamIdAt(pvpLobbyOf(game), 0);

  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t0)));

  const auto lobby = pvpLobbyFrame(a);
  REQUIRE(lobby.teams[0].has_value());
  REQUIRE(lobby.teams[0]->players[0].has_value());
  REQUIRE(lobby.teams[0]->players[1].has_value());
  CHECK_EQ(lobby.teams[0]->players[0]->id, 1u);
  CHECK_EQ(lobby.teams[0]->players[1]->id, 2u);
  CHECK_EQ(lobby.teams[0]->players[1]->is_ready, true);
}

/// Ids the match doesn't have: 0, 255, -1 (255 as a `uint8_t`), and the
/// first id no engaged team carries. Outcome isn't pinned, only that the
/// roster survives.
TEST(a_switch_to_an_unknown_team_id_is_rejected) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(aData));

  const auto before = pvpLobbyOf(game);
  const auto t0 = teamIdAt(before, 0);
  const auto t1 = teamIdAt(before, 1);

  long long unused = 1;
  while (unused == t0 || unused == t1)
    ++unused;

  for (const long long id : {0LL, 255LL, -1LL, unused}) {
    if (id == t0 || id == t1)
      continue;
    game.handleClientMessage(aData, switchTeamFrame(1, id));
  }

  const auto lobby = pvpLobbyOf(game);
  CHECK(teamOf(lobby, 1) == std::optional{t0});
  CHECK_EQ(shared::count_optional(lobby.teams), 2u);
  CHECK_EQ(seatsTaken(lobby, t0) + seatsTaken(lobby, t1), 1u);
}

TEST(a_switch_frame_missing_a_field_is_reported_as_a_bad_envelope) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(aData));
  const auto t1 = teamIdAt(pvpLobbyOf(game), 1);

  for (const auto &payload : {nlohmann::json{{"target_team_id", t1}},
                              nlohmann::json{{"player_id", 1}}}) {
    nlohmann::json envelope{};
    envelope["type"] = shared::ClientMessageType::SWITCH_TEAM;
    envelope["payload"] = payload;

    const auto outcome = game.handleClientMessage(
        aData, shared::encode(envelope, shared::WireFormat::JSON));
    CHECK_EQ(outcome.result, Result::BadEnvelope);
  }
}

/// Coop has no teams: the frame is harmless there. Outcome isn't pinned.
TEST(a_switch_frame_on_a_coop_game_changes_nothing) {
  FakeConnection a, b;
  CoopGame game;

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));

  game.handleClientMessage(aData, switchTeamFrame(1, 2));

  REQUIRE(game.findPlayerById(1) != nullptr);
  REQUIRE(game.findPlayerById(2) != nullptr);
  CHECK(!game.findPlayerById(1)->team_id.has_value());
  CHECK(!game.isRunning());
}

/// Guard only: a running PvP match always has full teams, so this can't
/// tell "ignored because running" from "rejected because full".
TEST(a_switch_during_a_running_match_changes_nothing) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));
  const auto t1 = teamIdAt(pvpLobbyOf(game), 1);
  REQUIRE(game.handleClientMessage(aData, readyFrame(1, true)));
  REQUIRE(game.handleClientMessage(bData, readyFrame(2, true)));
  REQUIRE(game.isRunning());

  game.handleClientMessage(aData, switchTeamFrame(1, t1));

  CHECK(game.isRunning());
  REQUIRE(testing::stepUntil(game, [] { return true; }));
  CHECK_EQ(testing::livePlayers(testing::pvpState(a)), 2u);
}

// --- Ready -------------------------------------------------------------

/// Why a switch clears ready: otherwise readying, then moving, lines up an
/// all-ready roster nobody agreed to.
TEST(a_switch_clears_ready_in_both_teams_it_touches) {
  FakeConnection a, b, c;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  auto cData = fakeConnection(c, {3});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData)); // team 0 is now full
  REQUIRE(game.addPlayers(cData)); // team 1
  const auto t1 = teamIdAt(pvpLobbyOf(game), 1);
  REQUIRE(game.handleClientMessage(aData, readyFrame(1, true)));
  REQUIRE(game.handleClientMessage(bData, readyFrame(2, true)));
  REQUIRE(game.handleClientMessage(cData, readyFrame(3, true)));
  REQUIRE(!game.isRunning());

  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t1)));

  const auto lobby = pvpLobbyFrame(c);
  for (const shared::PlayerId id : {1u, 2u, 3u}) {
    const auto player = lobbyPlayer(lobby, id);
    REQUIRE(player.has_value());
    CHECK_EQ(player->is_ready, false);
  }
  CHECK(!game.isRunning());
}

// --- Into the match ------------------------------------------------------

/// End to end: the sim's teams come from the roster at `tryStart`, so a
/// switched player starts the match on the team it switched to.
TEST(a_switched_player_starts_the_match_on_its_new_team) {
  FakeConnection a, b, c, d;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  auto aData = fakeConnection(a, {1});
  auto bData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(aData));
  REQUIRE(game.addPlayers(bData));
  const auto t1 = teamIdAt(pvpLobbyOf(game), 1);
  REQUIRE(game.handleClientMessage(aData, switchTeamFrame(1, t1)));

  // Auto-balance fills the gaps: 3 → team 0 beside 2, 4 → team 1 beside 1.
  auto cData = fakeConnection(c, {3});
  auto dData = fakeConnection(d, {4});
  REQUIRE(game.addPlayers(cData));
  REQUIRE(game.addPlayers(dData));

  REQUIRE(game.handleClientMessage(aData, readyFrame(1, true)));
  REQUIRE(game.handleClientMessage(bData, readyFrame(2, true)));
  REQUIRE(game.handleClientMessage(cData, readyFrame(3, true)));
  REQUIRE(game.handleClientMessage(dData, readyFrame(4, true)));
  REQUIRE(game.isRunning());

  REQUIRE(testing::stepUntil(game, [] { return true; }));
  const auto state = testing::pvpState(a);
  CHECK_EQ(testing::livePlayers(state), 4u);

  const auto *team = testing::teamById(state, t1);
  REQUIRE(team != nullptr);

  std::size_t found = 0;
  for (const auto &player : team->players)
    if (player && (player->id == 1 || player->id == 4))
      ++found;
  CHECK_EQ(found, 2u);
}

/// The point of per-player teams: one machine, both sides of a 1v1.
TEST(a_same_machine_duo_can_play_an_online_1v1) {
  FakeConnection a;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};

  auto aData = fakeConnection(a, {1, 2});
  REQUIRE(game.addPlayers(aData));

  REQUIRE(game.handleClientMessage(aData, readyFrame(1, true)));
  CHECK(!game.isRunning());
  REQUIRE(game.handleClientMessage(aData, readyFrame(2, true)));
  REQUIRE(game.isRunning());

  REQUIRE(testing::stepUntil(game, [] { return true; }));
  const auto state = testing::pvpState(a);
  CHECK_EQ(testing::livePlayers(state), 2u);
}
