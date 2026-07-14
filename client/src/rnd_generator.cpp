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

float RndGenerator::getRandomFloat(float min, float max) {
  return std::uniform_real_distribution<float>(min, max)(RNG_);
}
