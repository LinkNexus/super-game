#pragma once

#include <random>

namespace shared {
/// Process-wide random number source for gameplay randomness (enemy shot
/// timing, etc.). A single shared Mersenne Twister rather than per-caller
/// engines, seeded once via seed().
class RndGenerator {
  inline static std::mt19937 RNG_{};

public:
  /// Seeds the shared engine from a non-deterministic source. Called once
  /// per GameSim::start(), so each match gets its own random sequence.
  static void seed() {
    std::random_device rd;
    RNG_.seed(rd());
  }

  template <typename T>
  static T getRandom(T min, T max) {
    if constexpr (std::floating_point<T>)
      return std::uniform_real_distribution<T>(min, max)(RNG_);
    else
      return std::uniform_int_distribution<T>(min, max)(RNG_);
  }
};
} // namespace shared
