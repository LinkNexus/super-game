/// Client-to-server cases: `Game::handleClientMessage()` driven with real
/// encoded frames.
///
/// This is the direction the suite could not reach until L79. `test_lobby.cpp`
/// and `test_match.cpp` cover the *outbound* half - what the server puts on
/// the wire - but every inbound case called `setPlayersReady()` directly,
/// skipping decode and dispatch entirely, because those lived in a lambda
/// inside `main.cpp` that `supergame-server-lib` cannot link. That blind spot
/// is what let `auto j{shared::decode(message)}` ship: the braces selected
/// `nlohmann::json`'s `initializer_list` constructor, wrapping the decoded
/// object in a one-element array, so `j.at("type")` threw `type_error.304`
/// into a blind `catch` and every inbound frame was dropped in silence - with
/// a fully green suite, since no case ever fed the server a frame.
///
/// So the cases here start from bytes, not from a method call, and the
/// rejection cases assert on the returned `ClientMessageOutcome` rather than
/// on captured output: a dropped frame has to be distinguishable from a
/// handled one *by value*, which is the property that was missing.

#include "fake_connection.h"
#include "game.h"
#include "nlohmann/json.hpp"
#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/wire.h"
#include "test_harness.h"
#include <cstddef>
#include <initializer_list>
#include <string>

using testing::FakeConnection;
using testing::fakeConnection;

namespace {

using Result = Game::ClientMessageResult;

/// A READY frame exactly as the client builds one (`client/src/session.cpp`):
/// a `{type, payload}` envelope, encoded through the same `shared::encode`
/// both ends share, so a change to `WIRE_BINARY` moves these cases with it
/// instead of leaving them asserting against a format nobody speaks.
std::string readyFrame(bool is_ready) {
  nlohmann::json envelope{};
  envelope["type"] = shared::ClientMessageType::READY;
  envelope["payload"] = shared::ReadyMessage{is_ready};

  return shared::encode(envelope);
}

std::string inputFrame(std::initializer_list<shared::PlayerInput> inputs) {
  shared::PlayerInputs<shared::MAX_PLAYERS_PER_CLIENT> payload{};

  std::size_t i = 0;
  for (const auto &input : inputs)
    payload[i++] = input;

  nlohmann::json envelope{};
  envelope["type"] = shared::ClientMessageType::PLAYER_INPUT;
  envelope["payload"] = payload;

  return shared::encode(envelope);
}

/// The lobby snapshot carried by @p connection's most recent frame - the
/// counterpart of `coopState()` in match_support.h, and the reason the ready
/// cases below assert on the broadcast rather than on `findPlayerById()`:
/// what the other clients receive is the contract, and a ready toggle that
/// updated the roster without broadcasting would still read as "ready does
/// nothing" on every screen.
///
/// **Bind the result to a named local.** It is returned by value, and
/// `CHECK_EQ` binds a reference to its argument - so `CHECK_EQ(coopLobbyFrame(b)
/// .players[0]->is_ready, true)` reads a temporary that died at the semicolon,
/// because `std::optional::operator->` is a function call and so defeats
/// lifetime extension. ASan caught exactly that while these cases were being
/// written, which is the same stack-use-after-scope L64 hit with
/// `coopLobby(game.buildLobbyUpdate())` - the helper's shape differs, the trap
/// does not.
shared::CoopGameLobbyUpdate coopLobbyFrame(const FakeConnection &connection) {
  return connection.lastEnvelope()
      .at("payload")
      .get<shared::CoopGameLobbyUpdate>();
}

/// Two coop players, one slot each, neither ready yet.
void joinCoopPair(CoopGame &game, PerSocketPlayers &first,
                  PerSocketPlayers &second) {
  REQUIRE(game.addPlayers(first));
  REQUIRE(game.addPlayers(second));
}

} // namespace

// --- The ready path, end to end ------------------------------------------

/// The direct regression for the array-wrapping bug. Every assertion here
/// passed before L79 when written against `setPlayersReady()`; none of them
/// did when the same intent arrived as bytes.
TEST(a_ready_frame_from_the_wire_marks_the_sender_ready) {
  FakeConnection a, b;
  CoopGame game;

  auto firstData = fakeConnection(a, {1});
  auto secondData = fakeConnection(b, {2});
  joinCoopPair(game, firstData, secondData);

  const auto outcome = game.handleClientMessage(firstData, readyFrame(true));

  CHECK_EQ(outcome.result, Result::OK);
  CHECK(outcome.detail.empty());

  const auto lobby = coopLobbyFrame(a);
  REQUIRE(lobby.players[0].has_value());
  REQUIRE(lobby.players[1].has_value());
  CHECK_EQ(lobby.players[0]->is_ready, true);
  CHECK_EQ(lobby.players[1]->is_ready, false);
}

/// Handling the frame is only half of it: the toggle has to reach the *other*
/// client, or a lobby shows its own ready state and nobody else's.
TEST(a_ready_frame_broadcasts_a_fresh_lobby_update_to_every_player) {
  FakeConnection a, b;
  CoopGame game;

  auto firstData = fakeConnection(a, {1});
  auto secondData = fakeConnection(b, {2});
  joinCoopPair(game, firstData, secondData);

  const auto before = b.sent.size();

  REQUIRE(game.handleClientMessage(firstData, readyFrame(true)));

  CHECK(b.sent.size() > before);
  CHECK_EQ(b.lastType(), shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE);

  const auto lobby = coopLobbyFrame(b);
  REQUIRE(lobby.players[0].has_value());
  CHECK_EQ(lobby.players[0]->is_ready, true);
}

/// `setPlayersReady` keys on the socket rather than on a player id, so one
/// SPACE press from a same-keyboard duo readies both of their slots - and the
/// frame carries no player id at all, which is what makes that work.
TEST(a_ready_frame_readies_both_slots_of_a_two_slot_connection) {
  FakeConnection a, b;
  CoopGame game;

  auto firstData = fakeConnection(a, {1, 2});
  auto secondData = fakeConnection(b, {3});
  joinCoopPair(game, firstData, secondData);

  REQUIRE(game.handleClientMessage(firstData, readyFrame(true)));

  const auto lobby = coopLobbyFrame(a);
  REQUIRE(lobby.players[0].has_value());
  REQUIRE(lobby.players[1].has_value());
  REQUIRE(lobby.players[2].has_value());
  CHECK_EQ(lobby.players[0]->is_ready, true);
  CHECK_EQ(lobby.players[1]->is_ready, true);
  CHECK_EQ(lobby.players[2]->is_ready, false);
}

/// The last ready frame is also what starts the match, so the inbound path
/// owns the LOBBY -> PLAYING transition on every client.
TEST(the_last_ready_frame_starts_the_match) {
  FakeConnection a, b;
  CoopGame game;

  auto firstData = fakeConnection(a, {1});
  auto secondData = fakeConnection(b, {2});
  joinCoopPair(game, firstData, secondData);

  REQUIRE(game.handleClientMessage(firstData, readyFrame(true)));
  CHECK_EQ(game.isRunning(), false);

  REQUIRE(game.handleClientMessage(secondData, readyFrame(true)));
  CHECK_EQ(game.isRunning(), true);

  const auto lobby = coopLobbyFrame(b);
  CHECK_EQ(lobby.game_started, true);
}

/// `handleClientMessage` is non-virtual on the base, so PvP reaches
/// `setPlayersReady` through a different override of the same call. Covered
/// because the two rosters are shaped differently (flat array vs. teams) and
/// the ready lookup has been wrong in exactly one of them before.
TEST(a_ready_frame_works_the_same_on_a_pvp_game) {
  FakeConnection a, b;
  PvPGame game{/*team_size=*/1, /*team_count=*/2};

  auto firstData = fakeConnection(a, {1});
  auto secondData = fakeConnection(b, {2});
  REQUIRE(game.addPlayers(firstData));
  REQUIRE(game.addPlayers(secondData));

  REQUIRE(game.handleClientMessage(firstData, readyFrame(true)));
  CHECK_EQ(game.isRunning(), false);

  REQUIRE(game.handleClientMessage(secondData, readyFrame(true)));
  CHECK_EQ(game.isRunning(), true);
}

// --- Input routing --------------------------------------------------------
//
// These read `pending_movement` / `pending_shots` through `findPlayerById()`
// rather than through a broadcast, because that is where the inbound path
// ends: `update()` consumes both into the sim and neither crosses the wire.
// What is under test is the routing - that a frame reaches the slot it names
// and no other - not what the simulation later does with it.

TEST(an_input_frame_routes_to_the_slot_it_names) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1, 2});
  REQUIRE(game.addPlayers(data));

  REQUIRE(game.handleClientMessage(
      data, inputFrame({shared::PlayerInput{shared::BUTTON_LEFT, 2}})));

  auto *first = game.findPlayerById(1);
  auto *second = game.findPlayerById(2);
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);

  CHECK_EQ(second->pending_movement, shared::BUTTON_LEFT);
  CHECK_EQ(first->pending_movement, shared::BUTTON_NONE);
}

/// Both slots of a same-keyboard duo move independently inside one frame -
/// the client packs both into a single `PlayerInputs` array per tick.
TEST(one_frame_carries_both_slots_of_a_two_slot_connection) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1, 2});
  REQUIRE(game.addPlayers(data));

  REQUIRE(game.handleClientMessage(
      data, inputFrame({shared::PlayerInput{shared::BUTTON_LEFT, 1},
                        shared::PlayerInput{shared::BUTTON_RIGHT, 2}})));

  CHECK_EQ(game.findPlayerById(1)->pending_movement, shared::BUTTON_LEFT);
  CHECK_EQ(game.findPlayerById(2)->pending_movement, shared::BUTTON_RIGHT);
}

/// A connection may only move the players it owns. `player_id` arrives from
/// the client and is therefore untrusted: the dispatch matches it against the
/// sender's own `PerSocketPlayers` slots first, so naming somebody else's
/// player is a no-op rather than remote control of their ship.
TEST(an_input_naming_a_player_the_sender_does_not_own_is_ignored) {
  FakeConnection a, b;
  CoopGame game;

  auto firstData = fakeConnection(a, {1});
  auto secondData = fakeConnection(b, {2});
  joinCoopPair(game, firstData, secondData);

  REQUIRE(game.handleClientMessage(
      firstData, inputFrame({shared::PlayerInput{shared::BUTTON_LEFT, 2}})));

  CHECK_EQ(game.findPlayerById(2)->pending_movement, shared::BUTTON_NONE);
  CHECK_EQ(game.findPlayerById(1)->pending_movement, shared::BUTTON_NONE);
}

/// An id belonging to nobody is dropped per-input, not per-frame: the valid
/// entry beside it still lands. A `return` instead of a `continue` here would
/// make one stale id silence a whole connection.
TEST(an_unknown_player_id_does_not_discard_the_rest_of_the_frame) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1, 2});
  REQUIRE(game.addPlayers(data));

  REQUIRE(game.handleClientMessage(
      data, inputFrame({shared::PlayerInput{shared::BUTTON_LEFT, 99},
                        shared::PlayerInput{shared::BUTTON_RIGHT, 2}})));

  CHECK_EQ(game.findPlayerById(2)->pending_movement, shared::BUTTON_RIGHT);
}

/// Held fire counts once per press, not once per frame. At 60 Hz the
/// difference is 1 shot versus 60, and the edge detection lives on the
/// inbound path (`prev_shoot_held`), not in the sim.
TEST(a_held_shoot_button_counts_as_one_shot_across_frames) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(data));

  const auto held = inputFrame({shared::PlayerInput{shared::BUTTON_SHOOT, 1}});
  REQUIRE(game.handleClientMessage(data, held));
  REQUIRE(game.handleClientMessage(data, held));
  REQUIRE(game.handleClientMessage(data, held));

  CHECK_EQ(game.findPlayerById(1)->pending_shots, 1);
}

/// Releasing and pressing again is a second shot - the counterpart of the
/// case above, and what makes `prev_shoot_held` a latch rather than a lock.
TEST(releasing_and_pressing_shoot_again_counts_as_a_second_shot) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(data));

  REQUIRE(game.handleClientMessage(
      data, inputFrame({shared::PlayerInput{shared::BUTTON_SHOOT, 1}})));
  REQUIRE(game.handleClientMessage(
      data, inputFrame({shared::PlayerInput{shared::BUTTON_NONE, 1}})));
  REQUIRE(game.handleClientMessage(
      data, inputFrame({shared::PlayerInput{shared::BUTTON_SHOOT, 1}})));

  CHECK_EQ(game.findPlayerById(1)->pending_shots, 2);
}

// --- Rejection ------------------------------------------------------------
//
// Four frames that must all be refused, each with a *different* result, since
// collapsing them into one silent `return` is what made the array-wrapping
// bug indistinguishable from a client sending junk.

TEST(a_truncated_frame_is_reported_as_not_json) {
  FakeConnection a, b;
  CoopGame game;

  auto firstData = fakeConnection(a, {1});
  auto secondData = fakeConnection(b, {2});
  joinCoopPair(game, firstData, secondData);

  const auto before = a.sent.size();
  const auto outcome = game.handleClientMessage(firstData, R"({"type":)");

  CHECK_EQ(outcome.result, Result::NotJson);
  CHECK(!outcome.detail.empty());

  // Dropped means dropped: no roster change, and nothing broadcast.
  CHECK_EQ(game.findPlayerById(1)->is_ready, false);
  CHECK_EQ(a.sent.size(), before);
}

TEST(an_envelope_without_a_type_is_reported_as_a_bad_envelope) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(data));

  const auto outcome =
      game.handleClientMessage(data, R"({"payload":{"is_ready":true}})");

  CHECK_EQ(outcome.result, Result::BadEnvelope);
  CHECK(!outcome.detail.empty());
  CHECK_EQ(game.findPlayerById(1)->is_ready, false);
}

/// The exact bug, reproduced from the outside: a correctly built envelope
/// wrapped in a one-element array, which is what `auto j{shared::decode(...)}`
/// produced on the server's own side. `.at("type")` on an array throws
/// `type_error.304`, so this must come back as a bad envelope - the one
/// result that means "our side mishandled a frame that was fine" - and
/// never as OK.
TEST(an_array_wrapped_envelope_is_reported_as_a_bad_envelope) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(data));

  nlohmann::json wrapped = nlohmann::json::array();
  wrapped.push_back(nlohmann::json::parse(readyFrame(true)));

  const auto outcome =
      game.handleClientMessage(data, shared::encode(wrapped));

  CHECK_EQ(outcome.result, Result::BadEnvelope);
  CHECK_EQ(game.findPlayerById(1)->is_ready, false);
}

/// `ClientMessageType` has no custom (de)serializer, so nlohmann round-trips
/// it as a raw integer and throws nothing on a value outside the enum - the
/// one rejection here that never raises an exception at all, and so the one
/// the `catch` alone could not have caught.
TEST(a_type_outside_the_enum_is_reported_as_unknown) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(data));

  nlohmann::json envelope{};
  envelope["type"] = 7;
  envelope["payload"] = nlohmann::json::object();

  const auto outcome = game.handleClientMessage(data, shared::encode(envelope));

  CHECK_EQ(outcome.result, Result::UnknownType);
  CHECK(!outcome.detail.empty());
  CHECK_EQ(game.findPlayerById(1)->is_ready, false);
}

/// A well-formed envelope whose payload is the *other* message's shape. The
/// `type` is valid, so this gets past the range check and fails inside the
/// payload conversion instead - the version-skew case, and the reason the
/// detail string is worth logging.
TEST(a_payload_that_does_not_match_its_type_is_reported_as_a_bad_envelope) {
  FakeConnection a;
  CoopGame game;

  auto data = fakeConnection(a, {1});
  REQUIRE(game.addPlayers(data));

  nlohmann::json envelope{};
  envelope["type"] = shared::ClientMessageType::READY;
  envelope["payload"] = "not an object";

  const auto outcome = game.handleClientMessage(data, shared::encode(envelope));

  CHECK_EQ(outcome.result, Result::BadEnvelope);
  CHECK(!outcome.detail.empty());
  CHECK_EQ(game.findPlayerById(1)->is_ready, false);
}
