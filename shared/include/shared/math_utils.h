#pragma once

#include "nlohmann/json.hpp"

namespace shared {
struct Vec2D {
  float x, y;

  Vec2D operator+(const Vec2D &other) const;

  Vec2D &operator+=(const Vec2D &other);

  Vec2D operator-(const Vec2D &other) const;

  Vec2D &operator-=(const Vec2D &other);

  Vec2D operator*(float scalar) const;

  Vec2D &operator*=(float scalar);

  float length() const;

  Vec2D normalized() const;

  Vec2D rotated(float angle) const;

  Vec2D lerp(const Vec2D &other, const float t) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vec2D, x, y)

bool rectIntersection(Vec2D pos_a, float hw_a, float hh_a, Vec2D pos_b,
                      float hw_b, float hh_b);

float to_rads(float degrees);
} // namespace shared
