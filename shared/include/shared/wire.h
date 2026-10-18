#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace shared {

inline constexpr bool WIRE_BINARY = false;

inline std::string encode(const nlohmann::json &envelope) {
  if constexpr (WIRE_BINARY) {
    const auto bytes = nlohmann::json::to_msgpack(envelope);
    return std::string(bytes.begin(), bytes.end());
  } else {
    return envelope.dump();
  }
}

inline nlohmann::json decode(std::string_view frame) {
  if constexpr (WIRE_BINARY) {
    return nlohmann::json::from_msgpack(frame.begin(), frame.end());
  } else {
    return nlohmann::json::parse(frame);
  }
}

} // namespace shared
