/// Cases that drive a started match through `Game::update()`: the broadcast
/// itself, the name patching applied on top of the sim's output, input
/// routing, and the terminal-phase transitions.
///
/// These were unreachable before the transport moved behind `WsConnection` -
/// `update()` dereferences the connection to send, so a fake socket that was
/// only an identity could not be stepped.

#include "fake_connection.h"
#include "game.h"
#include "game_manager.h"
#include "match_support.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "test_harness.h"

using testing::coopState;
using testing::FakeConnection;
using testing::fakeConnection;
using testing::findPlayer;
using testing::hasPlayerNamed;
using testing::livePlayers;
using testing::pvpState;
using testing::stepUntil;

namespace {

/// A running 2-player coop match, one slot per connection. Connection ids
/// default to 1/2 but are a parameter: the server hands out
/// `ServerPlayerId`s from a process-wide counter, so in any match after the
/// first they are *not* the sim's 1..N, and several bugs have lived in that
/// gap (L62).
void startCoopMatch(CoopGame &game, FakeConnection &first,
                    FakeConnection &second, ServerPlayerId firstId = 1,
                    ServerPlayerId secondId = 2) {
  REQUIRE(game.addPlayers(fakeConnection(first, {firstId})));
  REQUIRE(game.addPlayers(fakeConnection(second, {secondId})));

  game.setPlayersReady(&first, true);
  game.setPlayersReady(&second, true);
  REQUIRE(game.isRunning());
}

/// A running 1v1, one player per team. Connection ids are a parameter for the
/// same reason as `startCoopMatch`'s.
void startDuelMatch(PvPGame &game, FakeConnection &first,
                    FakeConnection &second, ServerPlayerId firstId = 1,
                    ServerPlayerId secondId = 2) {
  REQUIRE(game.addPlayers(fakeConnection(first, {firstId})));
  REQUIRE(game.addPlayers(fakeConnection(second, {secondId})));

  game.setPlayersReady(&first, true);
  game.setPlayersReady(&second, true);
  REQUIRE(game.isRunning());
}

} // namespace

TEST(coop_broadcasts_one_identical_state_frame_per_connection_each_tick) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b);

  // Joining and readying broadcast nothing on their own: a lobby update goes
  // out only when the connection handlers ask for one (L66 moved the payload
  // onto `Game`, not the decision of when to send it).
  CHECK_EQ(a.sent.size(), 0u);

  game.update(shared::FIXED_DT);

  CHECK_EQ(a.sent.size(), 1u);
  CHECK_EQ(b.sent.size(), 1u);
  CHECK_EQ(a.lastType(), shared::ServerMessageType::COOP_GAME_STATE);
  // Every recipient gets byte-identical bytes, which is what makes L69's
  // "hoist the dump() out of the per-socket loop" a safe change.
  CHECK_EQ(a.sent.back(), b.sent.back());
}

TEST(coop_broadcasts_nothing_before_the_match_starts) {
  FakeConnection a, b;
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));
  REQUIRE(game.isRunning() == false);

  game.update(shared::FIXED_DT);

  CHECK_EQ(a.sent.size(), 0u);
  CHECK_EQ(b.sent.size(), 0u);
}

TEST(pvp_broadcasts_a_pvp_state_frame_once_running) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};
  startDuelMatch(game, a, b);

  game.update(shared::FIXED_DT);

  CHECK_EQ(a.sent.size(), 1u);
  CHECK_EQ(b.sent.size(), 1u);
  CHECK_EQ(a.lastType(), shared::ServerMessageType::PVP_GAME_STATE);
  CHECK_EQ(a.sent.back(), b.sent.back());
}

// --- Names -----------------------------------------------------------------
// The sim is identity-agnostic: it fills PlayerState::id but never a name.
// Game::update() patches each connection's name on afterwards, which is the
// only reason an online client can label a ship.

TEST(coop_patches_each_players_name_into_the_broadcast) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b);

  game.update(shared::FIXED_DT);
  const auto state = coopState(a);

  REQUIRE(livePlayers(state) == 2u);

  const auto first = findPlayer(state, 1);
  const auto second = findPlayer(state, 2);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK_EQ(first->name, std::string("P1"));
  CHECK_EQ(second->name, std::string("P2"));
}

/// L62's bug (1): names used to be matched by comparing a *connection* id to
/// `PlayerState::id`, which carries the *sim* id. The two coincide only in
/// the first match after a process start, since connection ids come from a
/// process-wide counter while sim ids restart at 1 every match. A later match
/// must still resolve names.
TEST(coop_patches_names_in_a_later_match_where_ids_no_longer_coincide) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b, /*firstId=*/7, /*secondId=*/8);

  game.update(shared::FIXED_DT);
  const auto state = coopState(a);

  REQUIRE(livePlayers(state) == 2u);
  // Looked up by name rather than by id, so this case survives L62 unifying
  // the two namespaces - which changes *which* ids appear here but not that
  // both names must resolve. A missing patch still leaves them empty.
  CHECK(hasPlayerNamed(state, "P7"));
  CHECK(hasPlayerNamed(state, "P8"));
}

TEST(pvp_patches_names_into_both_teams) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};
  startDuelMatch(game, a, b);

  game.update(shared::FIXED_DT);
  const auto state = pvpState(a);

  const auto first = findPlayer(state, 1);
  const auto second = findPlayer(state, 2);
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK_EQ(first->name, std::string("P1"));
  CHECK_EQ(second->name, std::string("P2"));
}

// --- One id namespace (L62) ------------------------------------------------
//
// WELCOME is sent from `.open` and carries the *connection* ids
// (`GameManager::next_player_id++`, process-wide). `PlayerState::id` carries
// the *sim* id, which `tryStart()` only mints once a match forms and which
// restarts at 1 every match. The client compares the two to decide "(You)",
// ship colours and the end-screen roster, so those all degrade from the
// second match after a server restart onward.
//
// These cases encode the structural fix - `start()` takes the ids the server
// already holds, so there is one namespace and every comparison site becomes
// correct, the lobby included. They are RED until that lands.
//
// If you instead re-send WELCOME with sim ids at match start, these are the
// wrong assertions: flip them to "the ids WELCOME re-sent match
// PlayerState::id" and remember that the L72 lobby rosters, which compare
// against `PlayerInfo::id` (a connection id, filled in `sendLobbyUpdate`),
// go from correct to broken under that option.

TEST(coop_broadcast_ids_are_the_ids_welcome_handed_out) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b, /*firstId=*/7, /*secondId=*/8);

  game.update(shared::FIXED_DT);
  const auto state = coopState(a);

  REQUIRE(livePlayers(state) == 2u);
  CHECK(findPlayer(state, 7).has_value());
  CHECK(findPlayer(state, 8).has_value());
}

/// The same requirement read from both sides at once: one number must
/// address a player in the roster *and* in the broadcast. `PlayerConnection`
/// no longer carries a second `id_in_game` for this to disagree with, so this
/// is now a guard against the two namespaces growing back rather than a
/// reproduction of the original bug.
TEST(coop_one_id_addresses_both_the_roster_and_the_wire) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b, /*firstId=*/7, /*secondId=*/8);

  game.update(shared::FIXED_DT);
  const auto state = coopState(a);

  for (const ServerPlayerId id : {7u, 8u}) {
    CHECK(game.findPlayerById(id) != nullptr);
    CHECK(findPlayer(state, id).has_value());
  }
}

TEST(pvp_broadcast_ids_are_the_ids_welcome_handed_out) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};
  startDuelMatch(game, a, b, /*firstId=*/5, /*secondId=*/6);

  game.update(shared::FIXED_DT);
  const auto state = pvpState(a);

  CHECK(findPlayer(state, 5).has_value());
  CHECK(findPlayer(state, 6).has_value());
}

/// Input routing already keys on the connection id and so is correct today
/// (L32) - this pins that it stays correct once the ids unify, since the
/// `input.player_id` the sim matches on becomes the same number.
TEST(coop_input_still_reaches_the_right_ship_with_unified_ids) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b, /*firstId=*/7, /*secondId=*/8);

  game.update(shared::FIXED_DT);
  const auto before = coopState(a);
  const auto movedBefore = findPlayer(before, 7);
  const auto stillBefore = findPlayer(before, 8);
  REQUIRE(movedBefore.has_value());
  REQUIRE(stillBefore.has_value());

  auto *player = game.findPlayerById(7);
  REQUIRE(player != nullptr);
  player->pending_movement = shared::Button::BUTTON_RIGHT;

  game.update(shared::FIXED_DT);
  const auto after = coopState(a);
  const auto movedAfter = findPlayer(after, 7);
  const auto stillAfter = findPlayer(after, 8);
  REQUIRE(movedAfter.has_value());
  REQUIRE(stillAfter.has_value());

  CHECK(movedAfter->position.x > movedBefore->position.x);
  CHECK_EQ(stillAfter->position.x, stillBefore->position.x);
}

// --- Input routing ---------------------------------------------------------

TEST(coop_routes_a_held_movement_press_to_only_that_players_ship) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b);

  game.update(shared::FIXED_DT);
  const auto before = coopState(a);
  const auto movedBefore = findPlayer(before, 1);
  const auto stillBefore = findPlayer(before, 2);
  REQUIRE(movedBefore.has_value());
  REQUIRE(stillBefore.has_value());

  auto *player = game.findPlayerById(1);
  REQUIRE(player != nullptr);
  player->pending_movement = shared::Button::BUTTON_RIGHT;

  game.update(shared::FIXED_DT);
  const auto after = coopState(a);
  const auto movedAfter = findPlayer(after, 1);
  const auto stillAfter = findPlayer(after, 2);
  REQUIRE(movedAfter.has_value());
  REQUIRE(stillAfter.has_value());

  CHECK(movedAfter->position.x > movedBefore->position.x);
  CHECK_EQ(stillAfter->position.x, stillBefore->position.x);
}

TEST(coop_moves_a_ship_the_other_way_on_a_left_press) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b);

  game.update(shared::FIXED_DT);
  const auto before = findPlayer(coopState(a), 1);
  REQUIRE(before.has_value());

  auto *player = game.findPlayerById(1);
  REQUIRE(player != nullptr);
  player->pending_movement = shared::Button::BUTTON_LEFT;

  game.update(shared::FIXED_DT);
  const auto after = findPlayer(coopState(a), 1);
  REQUIRE(after.has_value());

  CHECK(after->position.x < before->position.x);
}

/// A shoot press is counted, not sampled: `.message` increments
/// `pending_shots` on the edge, and one tick must consume exactly one of
/// them, so a press between ticks is never dropped and a held key never
/// becomes rapid fire.
TEST(coop_consumes_one_pending_shot_per_tick) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b);

  auto *player = game.findPlayerById(1);
  REQUIRE(player != nullptr);
  player->pending_shots = 2;

  game.update(shared::FIXED_DT);
  CHECK_EQ(player->pending_shots, 1);

  game.update(shared::FIXED_DT);
  CHECK_EQ(player->pending_shots, 0);

  game.update(shared::FIXED_DT);
  CHECK_EQ(player->pending_shots, 0);
}

// --- Disconnect during a running match (L57) -------------------------------

TEST(coop_disconnect_drops_the_player_from_the_broadcast_state) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b);

  game.update(shared::FIXED_DT);
  REQUIRE(findPlayer(coopState(b), 1).has_value());

  game.removePlayers(&a);
  game.update(shared::FIXED_DT);

  const auto state = coopState(b);
  CHECK_EQ(livePlayers(state), 1u);
  // The departed player must be gone from the live roster, and the survivor
  // must still be there. Checked by id rather than slot on purpose: the sim
  // currently compacts on removal (L63), so a survivor's index can change
  // and asserting on it would test the layout instead of the requirement.
  CHECK(findPlayer(state, 1).has_value() == false);

  const auto survivor = findPlayer(state, 2);
  REQUIRE(survivor.has_value());
  CHECK(survivor->lives > 0);
  CHECK_EQ(survivor->name, std::string("P2"));
}

TEST(pvp_disconnect_frees_both_the_team_slot_and_the_socket) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};
  startDuelMatch(game, a, b);
  REQUIRE(game.getPlayerSockets().size() == 2u);

  game.removePlayers(&a);

  const auto &teams = game.getTeams();
  CHECK_EQ(shared::count_optional(teams[0]->players), 0);
  CHECK_EQ(shared::count_optional(teams[1]->players), 1);
  CHECK_EQ(game.getPlayerSockets().size(), 1u);
  CHECK(game.findPlayerById(1) == nullptr);
  CHECK(game.findPlayerById(2) != nullptr);
}

// --- Terminal phases -------------------------------------------------------

/// L57's severe case: `isTeamDead` counts an occupied slot with lives > 0 as
/// alive, so before the sim's `removePlayer` was wired into the disconnect
/// path a 1v1 disconnect stranded the match in PLAYERS_FIGHT with no winner
/// possible. The remaining team must win.
TEST(pvp_duel_ends_with_the_remaining_team_winning_after_a_disconnect) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};
  startDuelMatch(game, a, b);

  // The death check only runs once both teams have finished entering.
  REQUIRE(stepUntil(game, [&] {
    return pvpState(b).phase ==
           static_cast<uint8_t>(shared::PvPGameSim::Phase::PLAYERS_FIGHT);
  }));

  game.removePlayers(&a);
  game.update(shared::FIXED_DT);

  const auto state = pvpState(b);
  CHECK_EQ(state.phase, static_cast<uint8_t>(shared::PvPGameSim::Phase::END));
  CHECK_EQ(state.teams[1]->outcome,
           static_cast<uint8_t>(shared::PvPGameSim::TeamOutcome::WON));
  CHECK_EQ(state.teams[0]->outcome,
           static_cast<uint8_t>(shared::PvPGameSim::TeamOutcome::LOST));
  CHECK_EQ(game.isOver(), true);
  CHECK_EQ(game.isRunning(), false);
}

TEST(pvp_stops_broadcasting_once_the_match_is_over) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};
  startDuelMatch(game, a, b);

  REQUIRE(stepUntil(game, [&] {
    return pvpState(b).phase ==
           static_cast<uint8_t>(shared::PvPGameSim::Phase::PLAYERS_FIGHT);
  }));

  game.removePlayers(&a);
  game.update(shared::FIXED_DT);
  REQUIRE(game.isOver());

  const auto framesAtEnd = b.sent.size();
  game.update(shared::FIXED_DT);
  game.update(shared::FIXED_DT);

  CHECK_EQ(b.sent.size(), framesAtEnd);
}

TEST(coop_ends_when_its_last_player_disconnects) {
  FakeConnection a, b;
  CoopGame game;
  startCoopMatch(game, a, b);
  game.update(shared::FIXED_DT);

  game.removePlayers(&a);
  game.removePlayers(&b);
  game.update(shared::FIXED_DT);

  CHECK_EQ(game.isOver(), true);
  CHECK_EQ(game.isRunning(), false);
}

// --- Disconnect after the match is over (L76) ------------------------------
// A finished match still holds its connections: the winner's client sits on
// the end screen and closes whenever the player does. Releasing a connection
// must not be conditional on the match still running - the roster holds a
// `WsConnection *` into per-socket storage uWS frees the moment its close
// handler returns, and every broadcast is now a virtual call through it.

TEST(pvp_disconnect_after_the_match_is_over_still_frees_the_socket) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};
  startDuelMatch(game, a, b);

  REQUIRE(stepUntil(game, [&] {
    return pvpState(b).phase ==
           static_cast<uint8_t>(shared::PvPGameSim::Phase::PLAYERS_FIGHT);
  }));

  game.removePlayers(&a);
  game.update(shared::FIXED_DT);
  REQUIRE(game.isOver());

  // The winner now leaves the end screen.
  game.removePlayers(&b);

  CHECK_EQ(game.getPlayerSockets().size(), 0u);
  // Counted across engaged team slots rather than indexed: whether an
  // emptied team keeps its slot is the sim's business, not this case's.
  std::size_t rosterPlayers = 0;
  for (const auto &team : game.getTeams()) {
    if (team)
      rosterPlayers += shared::count_optional(team->players);
  }
  CHECK_EQ(rosterPlayers, 0u);
  CHECK(game.findPlayerById(2) == nullptr);
  // With no socket left the game reports empty, which is the only thing that
  // routes the close handler into destroyGame.
  CHECK_EQ(game.isEmpty(), true);
}

/// The close handler's own sequence, driven against the library: free the
/// departing connection's slots, then destroy the game if that emptied it.
/// Both halves have to hold on a match that has *already finished*, not only
/// on a live one.
TEST(manager_destroys_a_finished_duel_once_its_last_connection_closes) {
  FakeConnection a, b;
  GameManager manager;

  auto *game = manager.joinOrCreatePvPGame(fakeConnection(a, {1}), 1, 2);
  const auto *joined = manager.joinOrCreatePvPGame(fakeConnection(b, {2}), 1, 2);
  REQUIRE(game != nullptr);
  REQUIRE(joined == game);

  // Kept separately so the post-destroy assertions below never reach through
  // the handle.
  const auto id = game->id;

  game->setPlayersReady(&a, true);
  game->setPlayersReady(&b, true);
  REQUIRE(game->isRunning());

  REQUIRE(stepUntil(*game, [&] {
    return pvpState(b).phase ==
           static_cast<uint8_t>(shared::PvPGameSim::Phase::PLAYERS_FIGHT);
  }));

  // First close: the match ends on the following tick, but b is still
  // connected, so the game must survive it.
  game->removePlayers(&a);
  REQUIRE(game->isEmpty() == false);
  game->update(shared::FIXED_DT);
  REQUIRE(game->isOver());
  CHECK(manager.findGameById(id) != nullptr);

  // Second close, from the end screen. Nothing may touch `game` after the
  // destroy, so the emptiness check is read once, here.
  game->removePlayers(&b);
  const bool emptied = game->isEmpty();
  CHECK_EQ(emptied, true);

  if (emptied)
    manager.destroyGame(id);

  CHECK(manager.findGameById(id) == nullptr);
}

// --- Joining a match that is no longer open --------------------------------

/// L28's bug (3): a player matched into an already-running coop game joined
/// as a shipless spectator, because `sim_.start()` had run without their id.
TEST(coop_rejects_a_joiner_once_the_match_is_running) {
  FakeConnection a, b, late;
  CoopGame game;
  startCoopMatch(game, a, b);

  CHECK_EQ(game.addPlayers(fakeConnection(late, {3})), false);
  CHECK_EQ(shared::count_optional(game.getPlayers()), 2);
  CHECK_EQ(game.getPlayerSockets().size(), 2u);
}

TEST(coop_rejects_a_joiner_once_the_match_is_over) {
  FakeConnection a, b, late;
  CoopGame game;
  startCoopMatch(game, a, b);

  game.removePlayers(&a);
  game.removePlayers(&b);
  game.update(shared::FIXED_DT);
  REQUIRE(game.isOver());

  CHECK_EQ(game.addPlayers(fakeConnection(late, {3})), false);
}

TEST(pvp_rejects_a_joiner_once_the_match_is_running) {
  FakeConnection a, b, late;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};
  startDuelMatch(game, a, b);

  CHECK_EQ(game.addPlayers(fakeConnection(late, {3})), false);
  CHECK_EQ(game.getPlayerSockets().size(), 2u);
}
