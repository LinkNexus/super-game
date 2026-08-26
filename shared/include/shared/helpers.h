#pragma once

#include <charconv>
#include <optional>
#include <string_view>

namespace shared {
template <class... Ts> struct overloaded : Ts... {
  using Ts::operator()...;
};
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

inline std::optional<int> toInt(std::string_view sv) {
  int value;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

  if (ec != std::errc{} || ptr != sv.data() + sv.size())
    return std::nullopt;

  return value;
}
} // namespace shared
