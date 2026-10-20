#pragma once

#include "shared/aliases.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <functional>
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

template <typename T, std::size_t N>
inline PlayerCount count_optional(const std::array<std::optional<T>, N> &arr) {
  return static_cast<PlayerCount>(std::count_if(
      arr.begin(), arr.end(), [](const auto &opt) { return opt.has_value(); }));
}

template <typename T, std::size_t N, typename Fn>
  requires(std::is_invocable_v<Fn, T &, std::size_t>)
void iterateOverOptionals(std::array<std::optional<T>, N> &arr, Fn callback) {
  for (std::size_t i = 0; i < arr.size(); ++i) {
    if (arr[i].has_value()) {
      callback(arr[i].value(), i);
    }
  }
}

template <typename T, std::size_t N, typename Fn>
  requires(std::is_invocable_v<Fn, const T &, std::size_t>)
void iterateOverOptionals(const std::array<std::optional<T>, N> &arr,
                          Fn callback) {
  for (std::size_t i = 0; i < arr.size(); ++i) {
    if (arr[i].has_value()) {
      callback(arr[i].value(), i);
    }
  }
}
} // namespace shared
