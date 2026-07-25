#pragma once

#include "constants.h"
#include "entities/boss.h"
#include "entities/player.h"
#include "entities/star.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <array>

enum class Screen { MENU, PLAYING, PAUSED, GAME_OVER, WIN };

class Game {
public:
  void run();

private:
  void init();
  void update(float dt);
  void draw();
  void handleInput();
  shared::Button getPlayerInputs();

  Player player_;
  Boss boss_;
  std::array<Star, STAR_COUNT> stars_;
  Screen screen_;
  shared::GameSim sim_;
  shared::GameState state_;
};
