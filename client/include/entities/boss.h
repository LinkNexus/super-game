#pragma once
#include "raylib.h"
#include "shared/messages.h"

/// Render-only boss entity: texture + health bar, no simulation logic.
/// Draws from the trimmed wire `BossState`, identically whether it came
/// from local sim or a network snapshot.
struct Boss {
  /// Draws the boss sprite at `state.position`, and its health bar (as a
  /// fraction of `state.max_health`) when @p draw_health_bar is true.
  void draw(const shared::BossState &state, bool draw_health_bar) const;
  void loadTexture();
  void unload();

  static constexpr float HEALTH_BAR_WIDTH = 300.0f;
  static constexpr float HEALTH_BAR_HEIGHT = 20.0f;
  static constexpr float HEALTH_BAR_Y = 10.0f;
  static constexpr float FINAL_POSITION_Y = 150.0f;
  static constexpr float INITIAL_DESCENT_SPEED = 70.0f;
  static constexpr float CYCLE_SPEED = 400.0f;

private:
  Texture2D texture_ = {};
};
