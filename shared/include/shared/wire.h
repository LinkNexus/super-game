#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <nlohmann/json.hpp>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace shared {

enum class WireFormat { JSON, MSGPACK };

static inline std::string encode(const nlohmann::json &envelope,
                                 WireFormat format) {
  switch (format) {
  case WireFormat::MSGPACK: {
    const auto bytes = nlohmann::json::to_msgpack(envelope);
    return std::string(bytes.begin(), bytes.end());
  }

  case WireFormat::JSON:
  default:
    return envelope.dump();
  }
}

static inline nlohmann::json decode(std::string_view frame, WireFormat format) {
  switch (format) {
  case WireFormat::MSGPACK:
    return nlohmann::json::from_msgpack(frame.begin(), frame.end());

  case WireFormat::JSON:
  default:
    return nlohmann::json::parse(frame);
  }
}

static constexpr inline std::string_view subprotocolName(WireFormat format) {
  switch (format) {
  case WireFormat::MSGPACK:
    return "supergame.msgpack";

  case WireFormat::JSON:
  default:
    return "supergame.json";
  }
}

static constexpr std::array<std::pair<std::string_view, WireFormat>, 2>
    wireFormatSubprotocols = {
        std::pair{"supergame.json", WireFormat::JSON},
        std::pair{"supergame.msgpack", WireFormat::MSGPACK},
};

static inline std::optional<WireFormat>
negotiateWireFormat(const std::string_view offered) {
  if (offered.empty())
    return WireFormat::JSON;

  auto protocols =
      offered | std::views::split(',') | std::views::transform([](auto &&sv) {
        const auto first = std::ranges::find_if_not(sv, [](const auto c) {
          return std::isspace(static_cast<unsigned char>(c));
        });

        const auto last = std::ranges::find_if_not(
            sv | std::views::reverse, [](const auto c) {
              return std::isspace(static_cast<unsigned char>(c));
            });

        if (first == sv.end())
          return std::string_view{};

        return std::string_view{
            std::to_address(first),
            static_cast<std::size_t>(std::distance(first, last.base()))};
      });

  for (const auto &protocol : protocols) {
    auto it = std::find_if(
        wireFormatSubprotocols.begin(), wireFormatSubprotocols.end(),
        [&protocol](const auto &p) { return p.first == protocol; });

    if (it != wireFormatSubprotocols.end())
      return it->second;
  }

  return std::nullopt;
}

} // namespace shared
