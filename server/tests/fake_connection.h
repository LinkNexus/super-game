#pragma once

/// Fake connections for the server's tests.
///
/// With the transport behind `WsConnection`, a test hands `Game` a recording
/// implementation instead of a real socket. That makes every broadcast path
/// drivable - including `update()`, which was unreachable while the roster
/// held raw uWS pointers - and makes the frames the server would have put on
/// the wire inspectable as JSON.
///
/// Deliberately the only place the suite builds one, so a change to the
/// interface lands here rather than in every case.

#include "game.h"
#include "nlohmann/json.hpp"
#include "shared/messages.h"
#include "types.h"
#include <cstddef>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace testing {

/// A `WsConnection` that keeps every frame instead of writing it anywhere.
struct FakeConnection final : WsConnection {
  std::vector<std::string> sent{};

  void send(std::string_view payload) override { sent.emplace_back(payload); }

  /// The most recent frame, parsed as a `{type, payload}` envelope. Throws if
  /// nothing was sent or the frame isn't JSON - the runner reports either as
  /// a failed case rather than crashing the binary.
  nlohmann::json lastEnvelope() const {
    if (sent.empty())
      throw std::runtime_error("no frame was sent on this connection");

    return nlohmann::json::parse(sent.back());
  }

  shared::ServerMessageType lastType() const {
    return lastEnvelope().at("type").get<shared::ServerMessageType>();
  }
};

/// The payload `.upgrade`/`.open` hands to `addPlayers`: @p ids claimed as
/// slots on @p connection, named `P<id>` so roster assertions can tell them
/// apart. The ids are the server-global `ServerPlayerId`s that `.open`
/// normally draws from `GameManager::next_player_id`.
inline PerSocketPlayers
fakeConnection(FakeConnection &connection,
               std::initializer_list<ServerPlayerId> ids) {
  PerSocketPlayers data{};
  data.ws = &connection;
  data.count = static_cast<shared::PlayerCount>(ids.size());

  std::size_t i = 0;
  for (const auto id : ids) {
    data.players[i] = id;
    data.names[i] = "P" + std::to_string(id);
    ++i;
  }

  return data;
}

} // namespace testing
