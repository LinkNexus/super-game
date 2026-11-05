#pragma once

#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/bullet_sim.h"
#include "shared/types.h"
#include <array>
#include <mathlib.hpp>

namespace shared {
/// Full internal state for one player, owned by GameSim. Never crosses the
/// network verbatim - see PlayerState for the trimmed wire equivalent.
struct PlayerSimState {
  mathlib::Vec2f position;
  float fire_cooldown;
  PlayerLives lives;
  PlayerPoints points;
  float orientation;
  PlayerId id;

  static constexpr auto SIZE = 20.0f;
  static constexpr auto SPEED = 300.0f;
  /// Fraction of `SIZE` actually used as the collision hitbox - kept
  /// smaller than the visual size so grazes feel fair.
  static constexpr auto HITBOX_SCALE = 0.92f;
  static constexpr auto FIRE_COOLDOWN = 0.15f;
  static constexpr PlayerLives INITIAL_LIVES = 3;
  static constexpr float POSITION_Y = 60.0f;
  static constexpr float INITIAL_POSITION_X = SCREEN_WIDTH / 2.0f;

  /// Resets this player to their starting position/lives/points and
  /// assigns @p id.
  void init(PlayerId id,
            mathlib::Vec2f position = {INITIAL_POSITION_X, POSITION_Y},
            int lives = INITIAL_LIVES, float orientation = 0.0f);

  /// Activates the first free slot in @p bullets as a new player-owned
  /// bullet fired from this player's current position.
  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;

  /// Applies one tick of movement from @p input and, if @p can_fire_bullets
  /// is true and the fire cooldown has elapsed, spawns a bullet.
  void step(Button buttons, float dt,
            std::array<BulletSimState, MAX_BULLETS> &bullets,
            bool can_fire_bullets);

  [[nodiscard]]
  mathlib::Rectanglef getHitBox() const;
};
} // namespace shared
