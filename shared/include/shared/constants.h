#pragma once

#include <cstdint>
#include <string>

/// Engine-agnostic core shared verbatim between the client's local mode and
/// the headless server's authoritative simulation.
namespace shared {
/// Game world width in pixels, shared by simulation bounds and rendering.
static constexpr int SCREEN_WIDTH = 1000;
/// Game world height in pixels, shared by simulation bounds and rendering.
static constexpr int SCREEN_HEIGHT = 800;
/// Fixed simulation timestep (seconds) driving GameSim::step() at 60 Hz.
static constexpr float FIXED_DT = 1.0f / 60.0f;
/// Capacity of the fixed-size bullet pool shared by player/enemy/boss shots.
static constexpr int MAX_BULLETS = 64;
/// Score awarded to a player for each enemy or boss hit they land.
static constexpr int POINTS_PER_HIT = 10;
/// Maximum simultaneous players in one match, online or local co-op/pvp.
static constexpr uint8_t MAX_PLAYERS = 2;
/// Default WebSocket URL the client connects to when none is given on the
/// command line.
static std::string default_server_url = "wss://supergame.levynkeneng.dev";
} // namespace shared
