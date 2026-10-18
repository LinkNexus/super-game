#pragma once

#include "shared/aliases.h"
#include <cstdint>
#include <string_view>

static constexpr uint8_t TARGET_FPS = 144;
static constexpr uint8_t LP_FONT_SIZE = 20;
static constexpr uint8_t STAR_COUNT = 150;
static constexpr uint8_t STATUS_FONT_SIZE = 20;
static constexpr shared::PlayerCount MAX_PLAYERS_ON_THIS_MACHINE = 2;
static constexpr uint8_t MAX_PARTICLES = 128;
inline constexpr std::string_view default_server_url =
    "wss://supergame.levynkeneng.dev";
