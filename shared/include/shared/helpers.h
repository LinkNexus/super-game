#pragma once

#include <charconv>
#include <optional>
#include <string_view>

namespace shared {
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

struct PairHash {
  template <typename T, typename U>
  std::size_t operator()(const std::pair<T, U> &p) const {
    std::size_t h1 = std::hash<T>{}(p.first);
    std::size_t h2 = std::hash<U>{}(p.second);

    return h1 ^ (h2 << 1);
  }
};

inline std::optional<int> toInt(std::string_view sv) {
  int value;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

  if (ec != std::errc{} || ptr != sv.data() + sv.size())
    return std::nullopt;

  return value;
}
} // namespace shared
