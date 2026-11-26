#pragma once

#include "constants.h"
#include "mathlib.hpp"
#include "raylib.h"
#include "shared/sim/world.h"

constexpr auto bounds = shared::sim::FlatWorld::FIELD_BOUNDS;
constexpr auto fieldSize = bounds.max - bounds.min;
constexpr auto scale = SCREEN_WIDTH / fieldSize.x;

static constexpr inline mathlib::Vec3f toScreen(mathlib::Vec3f vec) {
  return mathlib::Vec3f{vec.x - bounds.min.x, vec.y - bounds.min.y, bounds.max.z - vec.z}.scaledBy(
      scale
  );
}

template <typename T>
static constexpr inline auto toScreenSize(T size) {
  return size * scale;
}

static constexpr inline auto toScreenSize(const mathlib::Vec3f size) {
  return size * scale;
}

static constexpr inline Vector2 toRaylibVec(mathlib::Vec3f vec) {
  const auto screenVec = toScreen(vec);
  return {screenVec.x, screenVec.z};
}
