#include <cstddef>
#include <variant>

namespace shared {
struct CoopMode {
  CoopMode(std::size_t players_count);
  std::size_t players_count_;
};

struct PvPMode {
  PvPMode(std::size_t players_per_team);
  std::size_t players_per_team_;
};

using Mode = std::variant<CoopMode, PvPMode>;
} // namespace shared
