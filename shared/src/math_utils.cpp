#include "shared/math_utils.h"
#include <cmath>
#include <numbers>

shared::Vec2D shared::Vec2D::operator+(const Vec2D &other) const {
  return {x + other.x, y + other.y};
}

shared::Vec2D &shared::Vec2D::operator+=(const Vec2D &other) {
  x += other.x;
  y += other.y;
  return *this;
}

shared::Vec2D shared::Vec2D::operator-(const Vec2D &other) const {
  return {x - other.x, y - other.y};
}

shared::Vec2D &shared::Vec2D::operator-=(const Vec2D &other) {
  x -= other.x;
  y -= other.y;
  return *this;
}

shared::Vec2D shared::Vec2D::operator*(float scalar) const {
  return {x * scalar, y * scalar};
}

shared::Vec2D &shared::Vec2D::operator*=(float scalar) {
  x *= scalar;
  y *= scalar;
  return *this;
}

float shared::Vec2D::length() const { return std::sqrt(x * x + y * y); }

shared::Vec2D shared::Vec2D::normalized() const {
  float len = length();
  if (len == 0) {
    return {0, 0};
  }
  return {x / len, y / len};
}

shared::Vec2D shared::Vec2D::rotated(float angle) const {
  float cos_a = std::cos(angle);
  float sin_a = std::sin(angle);
  return {x * cos_a - y * sin_a, x * sin_a + y * cos_a};
}

shared::Vec2D shared::Vec2D::lerp(const Vec2D &other, const float t) const {
  return *this + (other - *this) * t;
}

bool shared::rectIntersection(shared::Vec2D pos_a, float hw_a, float hh_a,
                              shared::Vec2D pos_b, float hw_b, float hh_b) {
  return std::abs(pos_a.x - pos_b.x) <= (hw_a + hw_b) &&
         std::abs(pos_a.y - pos_b.y) <= (hh_a + hh_b);
}

float shared::to_rads(float degrees) {
  return degrees * std::numbers::pi / 180.0f;
}
