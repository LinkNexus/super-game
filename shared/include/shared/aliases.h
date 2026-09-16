#pragma once

#include "shared/constants.h"

namespace shared {

template <typename T>
using TypeInPlayerSlots = std::array<T, shared::MAX_PLAYERS>;

template <typename T>
using OptionalTypeInTeamSlots =
    std::array<std::array<std::optional<T>, shared::MAX_PLAYERS / 2>, 2>;

template <typename T>
using OptionalTypeInPlayerSlots = TypeInPlayerSlots<std::optional<T>>;

using PlayerId = uint32_t;
using PlayerIds = OptionalTypeInPlayerSlots<shared::PlayerId>;

struct PlayerInput;
using PlayerInputs = std::array<std::optional<PlayerInput>, MAX_PLAYERS>;

}; // namespace shared
