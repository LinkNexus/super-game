#pragma once

#include "mathlib.hpp"
#include "raylib.h"
#include "shared/constants.h"

static inline mathlib::Vec2f toScreen(mathlib::Vec2f vec) {
  return {vec.x, shared::SCREEN_HEIGHT - vec.y};
}

static inline Vector2 toRaylibVec(mathlib::Vec2f vec) {
  return {vec.x, shared::SCREEN_HEIGHT - vec.y};
}
