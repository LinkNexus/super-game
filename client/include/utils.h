#pragma once

#include "mathlib.hpp"
#include "raylib.h"
#include "shared/sim/constants.h"

static inline mathlib::Vec3f toScreen(mathlib::Vec3f vec) {
  return {vec.x, vec.y, shared::sim::SCREEN_HEIGHT - vec.z};
}

static inline Vector2 toRaylibVec(mathlib::Vec3f vec) {
  return {vec.x, shared::sim::SCREEN_HEIGHT - vec.z};
}
