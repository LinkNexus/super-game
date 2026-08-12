#include <random>

/// Process-wide random number source for gameplay randomness (enemy shot
/// timing, etc.). A single shared Mersenne Twister rather than per-caller
/// engines, seeded once via seed().
class RndGenerator {
  static std::mt19937 RNG_;

public:
  /// Seeds the shared engine from a non-deterministic source. Called once
  /// per GameSim::start(), so each match gets its own random sequence.
  static void seed();

  /// @return A uniformly distributed integer in the inclusive range
  /// [min, max].
  static int getRandomInt(int min, int max);

  /// @return A uniformly distributed float in the inclusive range
  /// [min, max].
  static float getRandomFloat(float min, float max);
};
