#pragma once

#include "shared/constants.h"
#include "shared/math_utils.h"
#include "shared/messages.h"
#include "shared/sim/bullet_sim.h"
#include <array>

namespace shared {
/// Full internal state for one player, owned by GameSim. Never crosses the
/// network verbatim - see PlayerState for the trimmed wire equivalent.
struct PlayerSimState {
  Vec2D position;
  float fire_cooldown;
  int lives;
  int points;
  /// Stable player identity used to match a PlayerInput to this player
  /// (by value, not by array slot) and to attribute bullet ownership.
  uint8_t id;

  static constexpr float SIZE = 20.0f;
  static constexpr float SPEED = 300.0f;
  /// Fraction of `SIZE` actually used as the collision hitbox - kept
  /// smaller than the visual size so grazes feel fair.
  static constexpr float HITBOX_SCALE = 0.80f;
  static constexpr float FIRE_COOLDOWN = 0.15f;
  static constexpr int INITIAL_LIVES = 3;
  static constexpr float POSITION_Y = SCREEN_HEIGHT - 60.0f;
  static constexpr float INITIAL_POSITION_X = SCREEN_WIDTH / 2.0f;

  /// Resets this player to their starting position/lives/points and
  /// assigns @p id.
  void init(uint8_t id);

  /// Activates the first free slot in @p bullets as a new player-owned
  /// bullet fired from this player's current position.
  void spawnBullet(std::array<BulletSimState, MAX_BULLETS> &bullets) const;

  /// Applies one tick of movement from @p input and, if @p can_fire_bullets
  /// is true and the fire cooldown has elapsed, spawns a bullet.
  void step(const PlayerInput &input, float dt,
            std::array<BulletSimState, MAX_BULLETS> &bullets,
            bool can_fire_bullets);
};
} // namespace shared
