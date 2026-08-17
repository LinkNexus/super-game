#include "shared/modes.h"
#include "shared/constants.h"
#include <stdexcept>
#include <string>

shared::CoopMode::CoopMode(std::size_t players_count) {
  if (players_count < 1 || players_count > shared::MAX_PLAYERS) {
    throw std::invalid_argument(
        "CoopMode: players_count must be between 1 and " +
        std::to_string(shared::MAX_PLAYERS));
  }

  players_count_ = players_count;
}

shared::PvPMode::PvPMode(std::size_t players_per_team) {
  if (players_per_team < 1 || players_per_team > shared::MAX_PLAYERS / 2) {
    throw std::invalid_argument(
        "PvPMode: players_per_team must be between 1 and " +
        std::to_string(shared::MAX_PLAYERS / 2));
  }

  players_per_team_ = players_per_team;
}
