#include "rnd_generator.h"
#include <random>

std::mt19937 RndGenerator::RNG_;

void RndGenerator::seed() {
  std::random_device rd;
  RNG_.seed(rd());
}

int RndGenerator::getRandomInt(int min, int max) {
  return std::uniform_int_distribution<int>(min, max)(RNG_);
}

float RndGenerator::getRandomFloat(float min, float max, int precision) {
  float scale = std::pow(10, precision);
  int scaled_min = static_cast<int>(min * scale);
  int scaled_max = static_cast<int>(max * scale);
  int random_scaled_value = getRandomInt(scaled_min, scaled_max);
  return static_cast<float>(random_scaled_value) / scale;
}
