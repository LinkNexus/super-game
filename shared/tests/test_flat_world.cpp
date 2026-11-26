/// Tests for `Flat`'s geometry on its own, without a sim (L85: the field is
/// centred on the origin, and everything placed in it follows).

#include "test_harness.h"

#include "shared/sim/constants.h"
#include "shared/sim/world.h"

#include <algorithm>
#include <array>
#include <numbers>

using namespace shared::sim;

namespace {

constexpr float TOLERANCE = 0.01f;

TEST(flat_field_is_centred_on_the_origin) {
  const auto field = FlatWorld::FIELD_BOUNDS;

  CHECK_NEAR(field.center().x, 0.0f, TOLERANCE);
  CHECK_NEAR(field.center().y, 0.0f, TOLERANCE);
  CHECK_NEAR(field.center().z, 0.0f, TOLERANCE);
}

/// Moving the origin must not change the field's shape.
TEST(flat_field_keeps_its_size) {
  const auto field = FlatWorld::FIELD_BOUNDS;

  CHECK_NEAR(field.max.x - field.min.x, FlatWorld::FIELD_WIDTH, TOLERANCE);
  CHECK_NEAR(field.max.y - field.min.y, 0.0f, TOLERANCE);
  CHECK_NEAR(field.max.z - field.min.z, FlatWorld::FIELD_DEPTH, TOLERANCE);
}

TEST(flat_player_spawn_is_on_the_centre_line) {
  CHECK_NEAR(
      FlatWorld::playerEntrance(0.0f).trajectory.end.x,
      FlatWorld::FIELD_BOUNDS.center().x,
      TOLERANCE
  );
}

/// The opposing team's spawn is the first team's turned 180° around y, which
/// with the origin at the centre is plain negation.
TEST(flat_opposite_player_spawns_mirror_through_the_origin) {
  const auto bottom = FlatWorld::playerEntrance(0.0f).trajectory.end;
  const auto top = FlatWorld::playerEntrance(std::numbers::pi_v<float>).trajectory.end;

  CHECK_NEAR(top.x, -bottom.x, TOLERANCE);
  CHECK_NEAR(top.y, -bottom.y, TOLERANCE);
  CHECK_NEAR(top.z, -bottom.z, TOLERANCE);
}

TEST(flat_boss_entrance_ends_on_the_centre_line) {
  CHECK_NEAR(
      FlatWorld::BOSS_ENTRANCE.trajectory.end.x, FlatWorld::FIELD_BOUNDS.center().x, TOLERANCE
  );
}

/// The formation's anchor is its far-left enemy's centre, so the far-left and
/// far-right centres must straddle the field's centre equally.
TEST(flat_formation_entrance_ends_centred) {
  for (EnemyCount rows = 1; rows <= MAX_ENEMY_ROWS; ++rows) {
    const auto left = FlatWorld::formationEntrance(rows).trajectory.end;
    const auto right = left + FlatWorld::formationSlot(0, ENEMY_COLS - 1);

    CHECK_NEAR((left.x + right.x) / 2.0f, FlatWorld::FIELD_BOUNDS.center().x, TOLERANCE);
  }
}

/// Fired from where the boss settles, the fan's outer shots aim at the
/// field's bottom corners, wherever the origin is.
TEST(flat_boss_spread_outer_shots_reach_the_bottom_corners) {
  const auto field = FlatWorld::FIELD_BOUNDS;
  const auto from = FlatWorld::BOSS_ENTRANCE.trajectory.end;

  std::array<mathlib::Vec3f, 5> directions{};
  FlatWorld::bossBulletsSpreadDirections(from, directions);

  // Where a shot from `from` along `d` crosses the field's bottom edge.
  const auto xAtBottom = [&](const mathlib::Vec3f &d) {
    const auto t = (field.min.z - from.z) / d.z;
    return from.x + t * d.x;
  };

  const auto first = xAtBottom(directions.front());
  const auto last = xAtBottom(directions.back());

  CHECK_NEAR(std::min(first, last), field.min.x, TOLERANCE);
  CHECK_NEAR(std::max(first, last), field.max.x, TOLERANCE);
}

} // namespace
