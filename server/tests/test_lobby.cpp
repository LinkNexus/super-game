/// Lobby-snapshot cases for `supergame-server-lib`.
///
/// These exist because the lobby payload used to be assembled in a free
/// function in `main.cpp`, behind a uWS socket, and so was the one broadcast
/// path no test could reach (L66). With the assembly on `Game` as
/// `buildLobbyUpdate()`, the roster snapshot is assertable as a struct - no
/// JSON, no fake socket - and the send path is assertable separately through
/// `broadcastLobbyUpdate()`.
///
/// Written against the `std::optional` slots rather than the `players_count` /
/// `max_players_count` / `team_count` fields on purpose: vacancy is the
/// optional now (L68), so these cases keep holding when those count fields
/// leave the wire.

#include "fake_connection.h"
#include "game.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "test_harness.h"
#include <variant>

using testing::FakeConnection;
using testing::fakeConnection;

namespace {

/// The coop/PvP alternative of a freshly built lobby snapshot, or nullptr if
/// the game produced the other one. Pointer rather than a reference so a
/// mismatch fails a `REQUIRE` instead of throwing `std::bad_variant_access`.
const shared::CoopGameLobbyUpdate *coopLobby(const shared::LobbyUpdate &update) {
  return std::get_if<shared::CoopGameLobbyUpdate>(&update);
}

const shared::PvPGameLobbyUpdate *pvpLobby(const shared::LobbyUpdate &update) {
  return std::get_if<shared::PvPGameLobbyUpdate>(&update);
}

/// `buildLobbyUpdate()` returns by value, and these hand back a pointer *into*
/// that variant - so calling them on the temporary directly leaves a dangling
/// pointer the moment the full expression ends. Deleting the rvalue overload
/// makes that a compile error rather than a use-after-scope that happens to
/// read the right bytes under one compiler and not the other.
const shared::CoopGameLobbyUpdate *coopLobby(shared::LobbyUpdate &&) = delete;
const shared::PvPGameLobbyUpdate *pvpLobby(shared::LobbyUpdate &&) = delete;

} // namespace

TEST(coop_lobby_lists_every_joined_player_by_slot_with_its_name) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = coopLobby(update);
  REQUIRE(lobby != nullptr);

  CHECK_EQ(shared::count_optional(lobby->players), 2);

  REQUIRE(lobby->players[0].has_value());
  CHECK_EQ(lobby->players[0]->id, 1u);
  CHECK_EQ(lobby->players[0]->name, std::string("P1"));
  CHECK_EQ(lobby->players[0]->is_ready, false);

  REQUIRE(lobby->players[1].has_value());
  CHECK_EQ(lobby->players[1]->id, 2u);
  CHECK_EQ(lobby->players[1]->name, std::string("P2"));

  CHECK_EQ(lobby->game_started, false);
}

/// The compaction the old `main.cpp` builder performed: it packed live players
/// to the front with its own `idx++` while the roster itself is sparse (L59),
/// so after a mid-lobby departure every remaining player shifted up a row and
/// the empty rows were drawn at the end. A slot belongs to its player.
TEST(coop_lobby_leaves_a_departed_players_slot_vacant) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1, 2})));
  REQUIRE(game.addPlayers(fakeConnection(b, {3})));

  game.removePlayers(&a);

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = coopLobby(update);
  REQUIRE(lobby != nullptr);

  CHECK_EQ(shared::count_optional(lobby->players), 1);
  CHECK_EQ(lobby->players[0].has_value(), false);
  CHECK_EQ(lobby->players[1].has_value(), false);

  REQUIRE(lobby->players[2].has_value());
  CHECK_EQ(lobby->players[2]->id, 3u);
}

TEST(coop_lobby_reports_a_ready_toggle_per_connection) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  game.setPlayersReady(&a, true);

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = coopLobby(update);
  REQUIRE(lobby != nullptr);

  REQUIRE(lobby->players[0].has_value());
  REQUIRE(lobby->players[1].has_value());
  CHECK_EQ(lobby->players[0]->is_ready, true);
  CHECK_EQ(lobby->players[1]->is_ready, false);
  CHECK_EQ(lobby->game_started, false);
}

/// Both slots of a two-slot connection ready together: `setPlayersReady` keys
/// on the socket, not on a player id, so one SPACE press from a same-keyboard
/// duo readies both of them.
TEST(coop_lobby_readies_both_players_of_a_two_slot_connection) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1, 2})));
  REQUIRE(game.addPlayers(fakeConnection(b, {3})));

  game.setPlayersReady(&a, true);

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = coopLobby(update);
  REQUIRE(lobby != nullptr);

  REQUIRE(lobby->players[0].has_value());
  REQUIRE(lobby->players[1].has_value());
  REQUIRE(lobby->players[2].has_value());
  CHECK_EQ(lobby->players[0]->is_ready, true);
  CHECK_EQ(lobby->players[1]->is_ready, true);
  CHECK_EQ(lobby->players[2]->is_ready, false);
}

/// `game_started` is what moves every client's screen from LOBBY to PLAYING,
/// so it has to track the game's own run state - the bug it replaced reported
/// `canStart()`, which is true one toggle *before* the match is running.
TEST(coop_lobby_reports_the_match_as_started_once_it_is_running) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  game.setPlayersReady(&a, true);
  game.setPlayersReady(&b, true);
  REQUIRE(game.isRunning());

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = coopLobby(update);
  REQUIRE(lobby != nullptr);
  CHECK_EQ(lobby->game_started, true);
}

/// The `emplace()`-inside-the-player-loop bug: `std::optional::emplace()`
/// destroys the contained value, so re-emplacing a team's array once per
/// player wiped slot 0 when slot 1 was written, and a same-keyboard duo saw
/// only one of themselves on their own team.
TEST(pvp_lobby_lists_both_players_of_a_two_player_team) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  REQUIRE(game.addPlayers(fakeConnection(a, {1, 2})));

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = pvpLobby(update);
  REQUIRE(lobby != nullptr);

  REQUIRE(lobby->teams[0].has_value());
  CHECK_EQ(shared::count_optional(*lobby->teams[0]), 2);

  REQUIRE((*lobby->teams[0])[0].has_value());
  REQUIRE((*lobby->teams[0])[1].has_value());
  CHECK_EQ((*lobby->teams[0])[0]->id, 1u);
  CHECK_EQ((*lobby->teams[0])[0]->name, std::string("P1"));
  CHECK_EQ((*lobby->teams[0])[1]->id, 2u);
  CHECK_EQ((*lobby->teams[0])[1]->name, std::string("P2"));
}

/// A team slot is vacant when the match has no such team, not when nobody has
/// joined it yet: `PvPGame`'s constructor engages every configured team, and
/// the lobby has to say so or the client's `if (!team) continue` silently drops
/// the half of a 2v2 that hasn't filled - no team header, no waiting rows.
TEST(pvp_lobby_keeps_a_configured_but_still_empty_team) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = pvpLobby(update);
  REQUIRE(lobby != nullptr);

  CHECK_EQ(shared::count_optional(lobby->teams), 2);

  REQUIRE(lobby->teams[1].has_value());
  CHECK_EQ(shared::count_optional(*lobby->teams[1]), 0);
}

/// A 1v1 engages two of the `MAX_TEAMS` slots, not all of them - the unused
/// ones stay vacant so the lobby renders two teams rather than the cap.
TEST(pvp_lobby_engages_only_the_teams_the_match_was_configured_with) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = pvpLobby(update);
  REQUIRE(lobby != nullptr);

  CHECK_EQ(shared::count_optional(lobby->teams), 2);
  CHECK_EQ(lobby->team_size, 1u);
}

/// `team_size` is a rule of the match, so it comes off the game. The builder
/// it replaced read it off the *requesting socket's* handshake params, which
/// is a second copy of the same fact and the thing L66 is about.
TEST(pvp_lobby_reports_the_games_own_team_size) {
  FakeConnection a;
  PvPGame game{/*team_size=*/2, /*team_count=*/2};

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));

  const auto update = game.buildLobbyUpdate();
  const auto *lobby = pvpLobby(update);
  REQUIRE(lobby != nullptr);
  CHECK_EQ(lobby->team_size, 2u);
}

TEST(coop_lobby_broadcast_reaches_every_connection_with_identical_bytes) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  game.broadcastLobbyUpdate();

  CHECK_EQ(a.sent.size(), 1u);
  CHECK_EQ(b.sent.size(), 1u);
  CHECK_EQ(a.lastType(), shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE);
  // Identical bytes per recipient is what makes the single hoisted `dump()`
  // correct (L69) - the payload does not depend on who receives it.
  CHECK_EQ(a.sent.back(), b.sent.back());
}

TEST(pvp_lobby_broadcast_carries_the_pvp_discriminator) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));

  game.broadcastLobbyUpdate();

  CHECK_EQ(a.lastType(), shared::ServerMessageType::PVP_GAME_LOBBY_UPDATE);
  CHECK_EQ(a.sent.back(), b.sent.back());
}

/// The discriminator the envelope carries has to be the one that decodes the
/// payload it was sent with. Both halves now come from the payload type itself
/// (`Payload::TYPE`), so this is the case that would catch them drifting apart
/// again - including after L69 swaps the encoding underneath.
TEST(coop_lobby_frame_decodes_back_into_the_snapshot_that_was_sent) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));
  game.setPlayersReady(&a, true);

  game.broadcastLobbyUpdate();

  const auto decoded =
      a.lastEnvelope().at("payload").get<shared::CoopGameLobbyUpdate>();
  const auto update = game.buildLobbyUpdate();
  const auto *built = coopLobby(update);
  REQUIRE(built != nullptr);

  CHECK_EQ(shared::count_optional(decoded.players),
           shared::count_optional(built->players));
  REQUIRE(decoded.players[0].has_value());
  REQUIRE(decoded.players[1].has_value());
  CHECK_EQ(decoded.players[0]->id, 1u);
  CHECK_EQ(decoded.players[0]->name, std::string("P1"));
  CHECK_EQ(decoded.players[0]->is_ready, true);
  CHECK_EQ(decoded.players[1]->is_ready, false);
  CHECK_EQ(decoded.game_started, false);
}

/// Nothing to send once the last connection is gone - and in particular no
/// crash from a roster that still holds a slot whose socket was erased.
TEST(coop_lobby_broadcast_is_a_no_op_once_every_connection_has_left) {
  FakeConnection a;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  game.removePlayers(&a);
  REQUIRE(game.isEmpty());

  game.broadcastLobbyUpdate();

  CHECK_EQ(a.sent.size(), 0u);
}
