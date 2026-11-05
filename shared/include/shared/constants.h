#pragma once

#include "shared/types.h"
#include <algorithm>
#include <cstdint>

/// Engine-agnostic core shared verbatim between the client's local mode and
/// the headless server's authoritative simulation.
namespace shared {
/// Game world width in pixels, shared by simulation bounds and rendering.
static constexpr uint16_t SCREEN_WIDTH = 1000;
/// Game world height in pixels, shared by simulation bounds and rendering.
static constexpr uint16_t SCREEN_HEIGHT = 800;
/// Fixed simulation timestep (seconds) driving GameSim::step() at 60 Hz.
static constexpr float FIXED_DT = 1.0f / 60.0f;
/// Capacity of the fixed-size bullet pool shared by player/enemy/boss shots.
static constexpr BulletCount MAX_BULLETS = 64;
/// Score awarded to a player for each enemy or boss hit they land.
static constexpr int POINTS_PER_HIT = 10;

static constexpr PlayerCount MAX_PLAYERS_COOP = 4;

static constexpr TeamCount MAX_TEAMS = 2;

static constexpr PlayerCount MAX_PLAYERS_PER_TEAM = 2;

static constexpr PlayerCount MAX_PLAYERS_PER_CLIENT = 2;

static constexpr EnemyCount MAX_ENEMY_ROWS = 8;

static constexpr EnemyCount ENEMY_COLS = 10;

static constexpr auto MAX_PLAYERS_PER_GAME =
    std::max<PlayerCount>(MAX_PLAYERS_COOP, MAX_TEAMS *MAX_PLAYERS_PER_TEAM);

constexpr std::size_t MAX_NAME_LENGTH = 9;
} // namespace shared
