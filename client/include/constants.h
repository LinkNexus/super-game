#pragma once

static constexpr int SCREEN_WIDTH = 1200;
static constexpr int SCREEN_HEIGHT = 800;
static constexpr int TARGET_FPS = 144;
static constexpr float FIXED_DT = 1.0f / 60.0f;

static constexpr int MAX_BULLETS = 64;

static constexpr int ENEMIES_ROWS = 5;
static constexpr int ENEMIES_COLS = 10;
static constexpr float ENEMIES_SPACING_X = 30.0f;
static constexpr float ENEMIES_SPACING_Y = 30.0f;
static constexpr float ENEMIES_CYCLE_SPEED = 200.0f;
static constexpr float ENEMIES_DESCENT_SPEED = 20.0f;
static constexpr float ENEMIES_SHOOTING_INTERVAL_MIN = 1.9f;
static constexpr float ENEMIES_SHOOTING_INTERVAL_MAX = 2.5f;

static constexpr float BULLET_SPEED = 600.0f;
static constexpr float FIRE_COOLDOWN = 0.15f;
