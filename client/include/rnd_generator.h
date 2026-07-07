#include <random>

class RndGenerator {
  static std::mt19937 RNG_;

public:
  static void seed();
  static int getRandomInt(int min, int max);
  static float getRandomFloat(float min, float max, int precision = 4);
};
