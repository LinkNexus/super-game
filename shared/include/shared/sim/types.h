#pragma once

#include <cstdint>
#include <mathlib.hpp>
#include <utility>

namespace shared::sim {
enum class Button : uint8_t {
  NONE = 0,
  LEFT = 1 << 0,
  RIGHT = 1 << 1,
  SHOOT = 1 << 2,
  UP = 1 << 3,
  DOWN = 1 << 4,
};

constexpr Button operator|(Button a, Button b) {
  return static_cast<Button>(std::to_underlying(a) | std::to_underlying(b));
}

constexpr Button operator|=(Button &a, Button b) {
  a = static_cast<Button>(std::to_underlying(a) | std::to_underlying(b));
  return a;
}

constexpr Button operator&(Button a, Button b) {
  return static_cast<Button>(std::to_underlying(a) & std::to_underlying(b));
}

constexpr bool buttonsHave(Button buttons, Button button) {
  return (buttons & button) != Button::NONE;
}

enum class CoopGamePhase : uint8_t {
  PLAYERS_ENTRANCE,
  ENEMIES_ENTRANCE,
  FIGHT_ENEMIES,
  BOSS_ENTRANCE,
  FIGHT_BOSS,
  WON,
  GAME_OVER
};

enum class PvPGamePhase : uint8_t { PLAYERS_ENTRANCE, PLAYERS_FIGHT, END };
enum class PvPGameTeamOutcome { PLAYING, WON, LOST, DREW };

enum class BulletType : uint8_t { ENEMY, PLAYER };
using BulletCount = uint8_t;

using PlayerId = uint32_t;
using PlayerCount = uint8_t;
template <std::size_t N>
using PlayerIds = std::array<std::optional<PlayerId>, N>;
using PlayerLives = uint8_t;
using PlayerPoints = uint32_t;

enum class EnemyType : uint8_t { TYPE_1, TYPE_2 };
using EnemyCount = uint8_t;
using EnemyDirection = int8_t;

using BossHealth = int32_t;
using BossDirection = int8_t;

using TeamCount = uint8_t;
using TeamId = uint8_t;

struct LinearEntrance {
  mathlib::LineSegment3f trajectory{};
  float speed{};
};
} // namespace shared::sim
