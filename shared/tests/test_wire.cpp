/// The wire format, chosen per connection (L82).
///
/// Every frame the server sends or accepts goes through `shared::encode` /
/// `shared::decode` with the format that connection negotiated, so the two
/// formats have to agree on the envelope they carry - a field that survives
/// JSON but not msgpack would break msgpack clients only. The negotiation
/// half reads the raw `Sec-WebSocket-Protocol` header the browser sends: the
/// subprotocols the client offers, in its order of preference.

#include "nlohmann/json.hpp"
#include "shared/wire.h"
#include "test_harness.h"
#include <optional>
#include <string>

namespace {

using shared::WireFormat;

/// A `{type, payload}` envelope with every JSON kind the wire structs use:
/// unsigned ids, signed and fractional numbers, booleans, strings, nested
/// objects, arrays and `null` (an empty `std::optional` slot).
nlohmann::json sampleEnvelope() {
  return {
      {"type", 3},
      {"payload",
       {{"players",
         {{{"id", 1u}, {"x", 412.5}, {"vx", -3}, {"name", "P1"}}, nullptr}},
        {"game_started", true}}},
  };
}

std::optional<WireFormat> negotiate(const char *offered) {
  return shared::negotiateWireFormat(offered);
}

} // namespace

// --- Round trips ------------------------------------------------------------

TEST(a_json_frame_decodes_back_to_the_same_envelope) {
  const auto envelope = sampleEnvelope();

  CHECK(shared::decode(shared::encode(envelope, WireFormat::JSON),
                       WireFormat::JSON) == envelope);
}

TEST(a_msgpack_frame_decodes_back_to_the_same_envelope) {
  const auto envelope = sampleEnvelope();

  CHECK(shared::decode(shared::encode(envelope, WireFormat::MSGPACK),
                       WireFormat::MSGPACK) == envelope);
}

/// JSON frames go out as text and are what the browser's network tab shows,
/// so they must stay plain `dump()` output.
TEST(a_json_frame_is_plain_json_text) {
  const auto envelope = sampleEnvelope();

  CHECK_EQ(shared::encode(envelope, WireFormat::JSON), envelope.dump());
}

TEST(a_msgpack_frame_is_not_json) {
  const auto frame = shared::encode(sampleEnvelope(), WireFormat::MSGPACK);

  CHECK(!nlohmann::json::accept(frame));
  CHECK(frame.size() < sampleEnvelope().dump().size());
}

/// A frame in the wrong format must throw, not decode into something else -
/// the server turns the throw into a dropped frame.
TEST(decoding_a_frame_with_the_other_format_throws) {
  const auto json = shared::encode(sampleEnvelope(), WireFormat::JSON);
  const auto msgpack = shared::encode(sampleEnvelope(), WireFormat::MSGPACK);

  bool msgpackAsJsonThrew = false;
  try {
    (void)shared::decode(msgpack, WireFormat::JSON);
  } catch (const nlohmann::json::exception &) {
    msgpackAsJsonThrew = true;
  }

  bool jsonAsMsgpackThrew = false;
  try {
    (void)shared::decode(json, WireFormat::MSGPACK);
  } catch (const nlohmann::json::exception &) {
    jsonAsMsgpackThrew = true;
  }

  CHECK(msgpackAsJsonThrew);
  CHECK(jsonAsMsgpackThrew);
}

// --- Negotiation ------------------------------------------------------------

/// No header at all: the desktop client, which offers no subprotocol.
TEST(no_offered_subprotocol_means_json) {
  CHECK(negotiate("") == WireFormat::JSON);
}

TEST(a_single_offered_subprotocol_is_picked) {
  CHECK(negotiate("supergame.json") == WireFormat::JSON);
  CHECK(negotiate("supergame.msgpack") == WireFormat::MSGPACK);
}

/// Browsers join the offered list with ", ", in the client's order of
/// preference - the first supported entry wins.
TEST(the_clients_first_supported_choice_wins) {
  CHECK(negotiate("supergame.msgpack, supergame.json") == WireFormat::MSGPACK);
  CHECK(negotiate("supergame.json, supergame.msgpack") == WireFormat::JSON);
}

TEST(the_offered_list_parses_without_spaces) {
  CHECK(negotiate("supergame.msgpack,supergame.json") == WireFormat::MSGPACK);
}

TEST(unknown_subprotocols_are_skipped) {
  CHECK(negotiate("chat, supergame.msgpack") == WireFormat::MSGPACK);
}

/// The client offered subprotocols and none of them is ours. Upgrading anyway
/// without echoing one makes the browser close the socket, so `.upgrade`
/// should reject instead.
TEST(only_unknown_subprotocols_is_no_match) {
  CHECK(!negotiate("chat").has_value());
  CHECK(!negotiate("chat, superchat").has_value());
}

/// `.upgrade` echoes `subprotocolName()` back; it has to be the exact token
/// the client offered, or the browser rejects the handshake.
TEST(subprotocol_names_negotiate_back_to_their_format) {
  for (const auto format : {WireFormat::JSON, WireFormat::MSGPACK}) {
    CHECK(negotiate(std::string(shared::subprotocolName(format)).c_str()) ==
          format);
  }

  CHECK_EQ(std::string(shared::subprotocolName(WireFormat::JSON)),
           std::string("supergame.json"));
  CHECK_EQ(std::string(shared::subprotocolName(WireFormat::MSGPACK)),
           std::string("supergame.msgpack"));
}
