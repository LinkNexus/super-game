#pragma once

#include "shared/constants.h"
#include "shared/messages.h"
#include "shared/sim/boss_sim.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/player_sim.h"

namespace shared {
enum class GamePhase : uint8_t {
  ENEMIES_ENTRANCE,
  FIGHT_ENEMIES,
  BOSS_ENTRANCE,
  FIGHT_BOSS,
  WON,
  GAME_OVER
};

class GameSim {
public:
  void start(std::array<std::optional<uint8_t>, MAX_PLAYERS> &player_ids);
  void step(GameState &state,
            const std::array<std::optional<PlayerInput>, MAX_PLAYERS> &inputs,
            float dt);

private:
  void checkCollisions();
  void setGameState(GameState &state);

  std::array<std::optional<PlayerSimState>, MAX_PLAYERS> players_{};
  std::array<BulletSimState, MAX_BULLETS> bullets_pool_{};
  EnemiesPoolSimState enemies_pool_{};
  BossSimState boss_{};
  GamePhase phase_{};
};
} // namespace shared
