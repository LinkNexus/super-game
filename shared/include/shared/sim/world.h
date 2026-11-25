#pragma once

#include "shared/sim/constants.h"
#include "shared/sim/entities/boss.h"
#include "shared/sim/entities/bullet.h"
#include "shared/sim/entities/enemies.h"
#include "shared/sim/entities/player.h"
#include "shared/sim/types.h"
#include <concepts>
#include <mathlib.hpp>
#include <span>

namespace shared::sim {

template <class W>
concept PlayerConstraints = requires(Button b, float orientation) {
  { W::playerEntrance(orientation) } -> std::same_as<LinearEntrance>;
  { W::playerBounds(orientation) } -> std::convertible_to<mathlib::Cuboidf>;
  { W::moveDirection(b) } -> std::same_as<mathlib::Vec3f>;
};

template <class W>
concept EnemyConstraints = requires(
    typename W::FormationMotion &motion,
    const mathlib::Cuboidf &extends,
    EnemyCount activeRows,
    EnemyCount enemyRow,
    EnemyCount enemyCol,
    float dt
) {
  { W::formationEntrance(activeRows) } -> std::same_as<LinearEntrance>;
  { W::formationSlot(enemyRow, enemyCol) } -> std::same_as<mathlib::Vec3f>;
  { W::moveFormation(motion, extends, dt) } -> std::same_as<mathlib::Vec3f>;
};

template <class W>
concept BossConstraints = requires(
    mathlib::Vec3f &bossPosition,
    const mathlib::Vec3f &dimensions,
    std::span<mathlib::Vec3f> outBullets,
    typename W::BossMotion &motion,
    float dt
) {
  { W::BOSS_ENTRANCE } -> std::convertible_to<LinearEntrance>;
  { W::bossBulletsSpreadDirections(bossPosition, outBullets) } -> std::same_as<void>;
  { W::moveBoss(motion, bossPosition, dt) } -> std::same_as<mathlib::Vec3f>;
};

template <class W>
concept World = PlayerConstraints<W> && EnemyConstraints<W> && BossConstraints<W> && requires {
  { W::BULLET_BOUNDS } -> std::convertible_to<mathlib::Cuboidf>;
};

static inline auto axis(Button buttons, Button negative, Button positive) {
  return (buttonsHave(buttons, positive) ? 1.f : 0.f) -
         (buttonsHave(buttons, negative) ? 1.f : 0.f);
}

struct Flat {
  struct FormationMotion {
    EnemyDirection direction{1};

    static constexpr auto CYCLE_SPEED = 200.0f;
    static constexpr auto DESCENT_SPEED = 35.0f;
  };

  struct BossMotion {
    BossDirection direction{1};

    static constexpr auto CYCLE_SPEED = 400.0f;
  };

  static constexpr auto FIELD_WIDTH = 1000.0f;
  static constexpr auto FIELD_DEPTH = 800.0f;
  // 1000 by 800 field
  static constexpr auto FIELD_BOUNDS =
      mathlib::Cuboidf::fromMinAndSize({0, 0, 0}, FIELD_WIDTH, 0, FIELD_DEPTH);

  static constexpr LinearEntrance playerEntrance(float orientation) {
    const auto playerZOffset = 60.0f;
    const auto playerFieldPerspective =
        mathlib::OrientedCuboidf(FIELD_BOUNDS)
            .rotateAroundCenter(-mathlib::Vec3f::unitY(), orientation);

    const auto end = playerFieldPerspective.pointAt({0.5f, 0.0f, playerZOffset / FIELD_DEPTH});
    const auto start =
        end -
        mathlib::Vec3f{0.0f, 0.0f, 100.0f}.rotatedAroundAxis(-mathlib::Vec3f::unitY(), orientation);

    return {.trajectory = mathlib::LineSegment3f(start, end), .speed = 50.0f};
  }

  // The player starts at the center and is pinned at 60 units above the bottom
  // of the screen
  static constexpr auto INITIAL_PLAYER_POSITION =
      mathlib::Vec3f{FIELD_BOUNDS.center().x - (PlayerSimState::SIZE.x / 2), 0, 60.0f};

  // The boss entrance starts from 30 units above the screen and ends at 81
  // units below the top of it.
  static constexpr auto BOSS_ENTRANCE = LinearEntrance{
      .trajectory = mathlib::LineSegment3f::fromStartAndDirection(
          {FIELD_BOUNDS.center().x, 0, FIELD_BOUNDS.max.z + 30.0f}, {0, 0, -111.0f}
      ),
      .speed = 70.0f
  };

  static constexpr LinearEntrance formationEntrance(EnemyCount activeRows) {
    constexpr auto width =
        (ENEMY_COLS * EnemySimState::SIZE.x) + (EnemiesPoolSimState::SPACING_X * (ENEMY_COLS - 1));

    // The formation starts in such a way that the bottom of it is at 30 units
    // above the top of the screen, regardless of the amounts of initial rows.
    // It also starts at the center horizontally.
    const auto depth = EnemiesPoolSimState::PITCH_Z * (activeRows - 1);
    const mathlib::Vec3f start = {
        FIELD_BOUNDS.center().x - (width + EnemySimState::SIZE.x) / 2.0f,
        0,
        FIELD_BOUNDS.max.z + 30.0f + depth
    };

    return {
        .trajectory =
            mathlib::LineSegment3f::fromStartAndDirection(start, {0, 0, -(depth + 80.0f)}),
        .speed = 70.0f
    };
  }

  // In the flat world, the player can only move in a one dimensional-fashion
  // along the x-axis, so we flatten to xy which -> x since y=0
  static constexpr mathlib::Cuboidf playerBounds(float orientation) {
    return FIELD_BOUNDS.flattenedToXY(playerEntrance(orientation).trajectory.end.z)
        .resizedBy({-PlayerSimState::SIZE.x, 0, 0});
  }

  static constexpr auto BULLET_BOUNDS = FIELD_BOUNDS.resizedBy(-BulletSimState::SIZE);

  // The enemies movement is 2 dimensional, cycling along the x-axis and
  // gradually coming down the z-one, so no need to flatten to xz since y is
  // already 0 in flat world
  static constexpr auto ENEMIES_BOUNDS = FIELD_BOUNDS.resizedBy(-EnemySimState::SIZE);

  // The boss movement is like the enemies' but is one dimensional along the
  // x-axis in a cycle-pattern. Hence, we flatten the bounds to just the xy
  // plane (which is in reality the x-plane since y = 0)
  static constexpr auto BOSS_BOUNDS = FIELD_BOUNDS.flattenedToXY(BOSS_ENTRANCE.trajectory.end.z)
                                          .resizeBy({-BossSimState::SIZE.x, 0, 0});

  static mathlib::Vec3f moveDirection(Button b) {
    return {axis(b, Button::LEFT, Button::RIGHT), 0, 0};
  }

  static mathlib::Vec3f formationSlot(EnemyCount row, EnemyCount col) {
    return {col * EnemiesPoolSimState::PITCH_X, 0, -row * EnemiesPoolSimState::PITCH_Z};
  }

  static mathlib::Vec3f
  moveFormation(FormationMotion &motion, const mathlib::Cuboidf &extends, float dt) {
    std::optional<mathlib::Vec3f> overflow = std::nullopt;

    if (!mathlib::contains(ENEMIES_BOUNDS, extends)) {
      motion.direction *= -1;
      overflow = mathlib::correction(ENEMIES_BOUNDS, extends);
    }

    return overflow ? overflow.value() + mathlib::Vec3f{0, 0, -FormationMotion::DESCENT_SPEED}
                    : mathlib::Vec3f{motion.direction * FormationMotion::CYCLE_SPEED * dt, 0, 0};
  }

  static void bossBulletsSpreadDirections(mathlib::Vec3f from, std::span<mathlib::Vec3f> out) {
    const auto bottomLeft = mathlib::Vec3f::zero();
    const auto bottomRight = mathlib::Vec3f::unitX() * FIELD_WIDTH;
    const auto straightDown = -mathlib::Vec3f::unitZ();

    const auto dir1 = (bottomLeft - from).normalize();
    const auto dir2 = (bottomRight - from).normalize();

    const auto angle = dir1.angleWith(dir2);

    for (std::size_t i = 0; i < out.size(); ++i) {
      auto angleOffset = 0.0f;
      if (out.size() > 1) {
        angleOffset = -angle / 2 + (angle / (out.size() - 1)) * i;
      }

      out[i] = straightDown.rotatedAroundAxis(-mathlib::Vec3f::unitY(), angleOffset);
    }
  }

  static mathlib::Vec3f moveBoss(BossMotion &motion, const mathlib::Vec3f &position, float dt) {
    std::optional<mathlib::Vec3f> overflow = std::nullopt;

    if (!BOSS_BOUNDS.containsPoint(position)) {
      motion.direction *= -1;
      overflow = BOSS_BOUNDS.closestPointTo(position);
    }

    return overflow
               ? overflow.value()
               : position + mathlib::Vec3f{motion.direction * BossMotion::CYCLE_SPEED * dt, 0, 0};
  }
};

} // namespace shared::sim
