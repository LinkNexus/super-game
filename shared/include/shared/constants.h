#pragma once

#include <cstdint>

namespace shared {
static constexpr int SCREEN_WIDTH = 1000;
static constexpr int SCREEN_HEIGHT = 800;
static constexpr float FIXED_DT = 1.0f / 60.0f;
static constexpr int MAX_BULLETS = 64;
static constexpr int POINTS_PER_HIT = 10;
static constexpr uint8_t MAX_PLAYERS = 2;
} // namespace shared
