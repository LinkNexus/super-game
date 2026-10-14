#pragma once

#include "shared/constants.h"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

using GameId = uint32_t;
using ServerPlayerId = uint32_t;

struct WsConnection {
  virtual ~WsConnection() = default;
  virtual void send(std::string_view payload) = 0;
};

struct PerSocketPlayers {
  std::array<std::optional<std::pair<ServerPlayerId, std::string>>,
             shared::MAX_PLAYERS_PER_CLIENT>
      players{};
  WsConnection *ws{};
};
