#include <random>

class RndGenerator {
  static std::mt19937 RNG_;

public:
  static void seed();
  static int getRandomInt(int min, int max);
};
