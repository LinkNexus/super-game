#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace shared {
using PlayerId = uint32_t;
using PlayerCount = uint8_t;

template <std::size_t N>
using PlayerIds = std::array<std::optional<PlayerId>, N>;
}; // namespace shared
