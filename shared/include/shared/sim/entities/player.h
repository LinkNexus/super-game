#pragma once

#include "shared/sim/constants.h"
#include "shared/sim/entities/bullet.h"
#include "shared/sim/types.h"
#include <array>
#include <mathlib.hpp>

namespace shared::sim {
/// Full internal state for one player, owned by GameSim. Never crosses the
/// network verbatim - see PlayerState for the trimmed wire equivalent.
struct PlayerSimState {
  mathlib::Vec3f position;
  float fire_cooldown;
  PlayerLives lives;
  PlayerPoints points;
  float orientation;
  PlayerId id;
  LinearEntrance entrance;

  static constexpr auto SIZE = mathlib::Vec3f{40, 40, 40};
  static constexpr auto SPEED = 300.0f;
  static constexpr auto HITBOX_SCALE = 0.92f;
  static constexpr auto FIRE_COOLDOWN = 0.15f;

  /// Resets this player to their starting position/lives/points and
  /// assigns @p id.
  void init(PlayerId id, LinearEntrance entrance, int lives,
            float orientation = 0.0f);

  /// Activates the first free slot in @p bullets as a new player-owned
  /// bullet fired from this player's current position.
  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;

  /// Applies one tick of movement from @p input and, if @p can_fire_bullets
  /// is true and the fire cooldown has elapsed, spawns a bullet.
  void step(const mathlib::Vec3f &direction, const mathlib::Cuboidf &bounds,
            float dt, std::array<BulletSimState, MAX_BULLETS> &bullets,
            bool isShooting, bool canShoot);

  [[nodiscard]]
  mathlib::Cuboidf getHitBox() const;
  void stepEntrance(float dt);
  bool isEntranceComplete() const;
};
} // namespace shared::sim
