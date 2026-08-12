#pragma once

#include "nlohmann/json.hpp"

namespace shared {
/// 2D vector used throughout the simulation for position/velocity, and sent
/// verbatim over the wire (JSON-serializable via the macro below).
struct Vec2D {
  float x, y;

  Vec2D operator+(const Vec2D &other) const;

  Vec2D &operator+=(const Vec2D &other);

  Vec2D operator-(const Vec2D &other) const;

  Vec2D &operator-=(const Vec2D &other);

  Vec2D operator*(float scalar) const;

  Vec2D &operator*=(float scalar);

  /// @return The Euclidean length of this vector.
  float length() const;

  /// @return A unit vector in the same direction, or {0, 0} if this vector
  /// has zero length (safe against division by zero).
  Vec2D normalized() const;

  /// @return This vector rotated by @p angle radians (counter-clockwise in
  /// standard math convention; screen-space Y is flipped, so visually this
  /// rotates clockwise).
  Vec2D rotated(float angle) const;

  /// @return The linear interpolation between this vector and @p other at
  /// parameter @p t (0 = this, 1 = other), used to smooth client-side
  /// rendering between received network snapshots.
  Vec2D lerp(const Vec2D &other, const float t) const;

  /// @return The dot product of this vector and @p other.
  float dot_product(const Vec2D &other) const;

  /// @return The unsigned angle in radians between this vector and
  /// @p other, in [0, pi].
  float angle_between(const Vec2D &other) const;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Vec2D, x, y)

/// Axis-aligned bounding box overlap test between two boxes given by their
/// center positions and half-width/half-height extents.
/// @return true if the boxes overlap.
bool rectIntersection(Vec2D pos_a, float hw_a, float hh_a, Vec2D pos_b,
                      float hw_b, float hh_b);

/// @return @p degrees converted to radians.
float to_rads(float degrees);
} // namespace shared
