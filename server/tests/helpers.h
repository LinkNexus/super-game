#include <algorithm>

namespace testing {
inline auto countPlayersInTeam(const auto &teamId, const auto &players) {
  return std::count_if(players.begin(), players.end(),
                       [&teamId](const auto &player) {
                         return player && player->team_id == teamId;
                       });
};
}; // namespace testing
