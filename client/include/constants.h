#pragma once

#include <cstdint>
#include <string_view>

static constexpr int TARGET_FPS = 144;
static constexpr int LP_FONT_SIZE = 20;
static constexpr int STAR_COUNT = 150;
static constexpr int STATUS_FONT_SIZE = 20;
static constexpr uint8_t MAX_PLAYERS_ON_THIS_MACHINE = 2;
static constexpr int MAX_PARTICLES = 128;
inline constexpr std::string_view default_server_url =
    "wss://supergame.levynkeneng.dev";
