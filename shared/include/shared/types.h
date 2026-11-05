#pragma once

#include <cstdint>
#include <mathlib.hpp>

namespace shared {
enum class CoopGamePhase : uint8_t {
  ENEMIES_ENTRANCE,
  FIGHT_ENEMIES,
  BOSS_ENTRANCE,
  FIGHT_BOSS,
  WON,
  GAME_OVER
};

enum class PvPGamePhase : uint8_t { PLAYERS_FIGHT, END };
enum class TeamOutcome { PLAYING, WON, LOST, DREW };

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

using TeamCount = uint8_t;
using TeamId = uint8_t;
} // namespace shared
