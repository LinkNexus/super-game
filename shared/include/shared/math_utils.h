#pragma once

#include <cmath>

namespace shared {
struct Vec2D {
  float x, y;

  Vec2D operator+(const Vec2D &other) const {
    return {x + other.x, y + other.y};
  }

  Vec2D &operator+=(const Vec2D &other) {
    x += other.x;
    y += other.y;
    return *this;
  }

  Vec2D operator-(const Vec2D &other) const {
    return {x - other.x, y - other.y};
  }

  Vec2D &operator-=(const Vec2D &other) {
    x -= other.x;
    y -= other.y;
    return *this;
  }

  Vec2D operator*(float scalar) const { return {x * scalar, y * scalar}; }

  Vec2D &operator*=(float scalar) {
    x *= scalar;
    y *= scalar;
    return *this;
  }

  float length() const { return std::sqrt(x * x + y * y); }

  Vec2D normalized() const {
    float len = length();
    if (len == 0) {
      return {0, 0};
    }
    return {x / len, y / len};
  }

  Vec2D rotated(float angle) const {
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    return {x * cos_a - y * sin_a, x * sin_a + y * cos_a};
  }
};
} // namespace shared
