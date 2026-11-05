/// One game, connections in different wire formats (L82).
///
/// The format is negotiated per connection, so a browser on msgpack and a
/// desktop client on JSON can sit in the same lobby. Every broadcast has to
/// reach each of them in its own format, and every inbound frame has to be
/// decoded in the sender's - a JSON frame on a msgpack connection is a
/// malformed frame, dropped like any other.

#include "fake_connection.h"
#include "game.h"
#include "match_support.h"
#include "nlohmann/json.hpp"
#include "shared/messages.h"
#include "shared/wire.h"
#include "test_harness.h"
#include <string>

using shared::WireFormat;
using testing::FakeConnection;
using testing::fakeConnection;
using testing::readyConnection;
using testing::stepUntil;

namespace {

std::string readyFrame(shared::PlayerId player_id, bool is_ready,
                       WireFormat format) {
  nlohmann::json envelope{};
  envelope["type"] = shared::ClientMessageType::READY;
  envelope["payload"] = {{"player_id", player_id}, {"is_ready", is_ready}};

  return shared::encode(envelope, format);
}

/// Player @p id's ready flag in @p connection's most recent lobby frame.
bool isReadyInLastLobby(const FakeConnection &connection, shared::PlayerId id) {
  const auto lobby = connection.lastEnvelope()
                         .at("payload")
                         .get<shared::CoopGameLobbyUpdate>();

  for (const auto &player : lobby.players) {
    if (player && player->id == id)
      return player->is_ready;
  }

  return false;
}

} // namespace

// --- Outbound ---------------------------------------------------------------

TEST(a_lobby_update_reaches_each_connection_in_its_own_format) {
  FakeConnection json{WireFormat::JSON};
  FakeConnection msgpack{WireFormat::MSGPACK};
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(json, {1})));
  REQUIRE(game.addPlayers(fakeConnection(msgpack, {2})));
  game.broadcastLobbyUpdate();

  REQUIRE(!json.sent.empty());
  REQUIRE(!msgpack.sent.empty());

  CHECK(nlohmann::json::accept(json.sent.back()));
  CHECK(!nlohmann::json::accept(msgpack.sent.back()));

  CHECK_EQ(json.lastType(), shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE);
  CHECK_EQ(msgpack.lastType(),
           shared::ServerMessageType::COOP_GAME_LOBBY_UPDATE);
  CHECK(json.lastEnvelope() == msgpack.lastEnvelope());
}

/// The 60 Hz path: `update()` broadcasts the state through the same template
/// as the lobby, but it is the frame that matters most for size.
TEST(a_match_state_reaches_each_connection_in_its_own_format) {
  FakeConnection json{WireFormat::JSON};
  FakeConnection msgpack{WireFormat::MSGPACK};
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(json, {1})));
  REQUIRE(game.addPlayers(fakeConnection(msgpack, {2})));
  readyConnection(game, &json, true);
  readyConnection(game, &msgpack, true);
  REQUIRE(game.isRunning());

  REQUIRE(stepUntil(game, [&] {
    return !msgpack.sent.empty() &&
           msgpack.lastType() == shared::ServerMessageType::COOP_GAME_STATE;
  }));

  CHECK_EQ(json.lastType(), shared::ServerMessageType::COOP_GAME_STATE);
  CHECK(!nlohmann::json::accept(msgpack.sent.back()));
  CHECK(msgpack.sent.back().size() < json.sent.back().size());
  CHECK(json.lastEnvelope() == msgpack.lastEnvelope());
}

/// Two msgpack connections: the frame is encoded once per format, so both
/// receive byte-identical frames.
TEST(connections_sharing_a_format_receive_identical_frames) {
  FakeConnection a{WireFormat::MSGPACK};
  FakeConnection b{WireFormat::MSGPACK};
  CoopGame game;

  REQUIRE(game.addPlayers(fakeConnection(a, {1})));
  REQUIRE(game.addPlayers(fakeConnection(b, {2})));
  game.broadcastLobbyUpdate();

  REQUIRE(!a.sent.empty());
  REQUIRE(!b.sent.empty());
  CHECK(a.sent.back() == b.sent.back());
}

// --- Inbound ----------------------------------------------------------------

TEST(a_msgpack_ready_frame_from_a_msgpack_connection_is_handled) {
  FakeConnection json{WireFormat::JSON};
  FakeConnection msgpack{WireFormat::MSGPACK};
  CoopGame game;

  auto jsonData = fakeConnection(json, {1});
  auto msgpackData = fakeConnection(msgpack, {2});
  REQUIRE(game.addPlayers(jsonData));
  REQUIRE(game.addPlayers(msgpackData));

  const auto outcome = game.handleClientMessage(
      msgpackData, readyFrame(2, true, WireFormat::MSGPACK));

  CHECK_EQ(outcome.result, Game::ClientMessageResult::OK);
  CHECK(isReadyInLastLobby(json, 2));
  CHECK(isReadyInLastLobby(msgpack, 2));
}

/// The sender's format decides how a frame is read, not the frame's look:
/// valid JSON on a msgpack connection is dropped, by value.
TEST(a_json_frame_on_a_msgpack_connection_is_dropped) {
  FakeConnection json{WireFormat::JSON};
  FakeConnection msgpack{WireFormat::MSGPACK};
  CoopGame game;

  auto jsonData = fakeConnection(json, {1});
  auto msgpackData = fakeConnection(msgpack, {2});
  REQUIRE(game.addPlayers(jsonData));
  REQUIRE(game.addPlayers(msgpackData));
  game.broadcastLobbyUpdate();

  const auto outcome = game.handleClientMessage(
      msgpackData, readyFrame(2, true, WireFormat::JSON));

  CHECK(!outcome);
  CHECK(!outcome.detail.empty());
  CHECK(!isReadyInLastLobby(json, 2));
}

TEST(a_msgpack_frame_on_a_json_connection_is_dropped) {
  FakeConnection json{WireFormat::JSON};
  FakeConnection msgpack{WireFormat::MSGPACK};
  CoopGame game;

  auto jsonData = fakeConnection(json, {1});
  auto msgpackData = fakeConnection(msgpack, {2});
  REQUIRE(game.addPlayers(jsonData));
  REQUIRE(game.addPlayers(msgpackData));
  game.broadcastLobbyUpdate();

  const auto outcome = game.handleClientMessage(
      jsonData, readyFrame(1, true, WireFormat::MSGPACK));

  CHECK(!outcome);
  CHECK(!isReadyInLastLobby(msgpack, 1));
}
