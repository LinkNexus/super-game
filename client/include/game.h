#pragma once

#include "constants.h"
#include "entities/boss.h"
#include "entities/player.h"
#include "entities/star.h"
#include "shared/messages.h"
#include "shared/sim/game_sim.h"
#include <array>
#include "raylib.h"

enum class Screen { MENU, CONNECTING, WAITING, PLAYING, PAUSED, GAME_OVER, WIN };

enum class GameMode { LOCAL, ONLINE };

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
  GameMode mode_ = GameMode::LOCAL;
  float connection_timer_ = 0.0f;
  shared::GameSim sim_;
  shared::GameState state_;
  shared::GameState prev_state_;

  // audio
  Sound shoot_sfx_;
  Sound explosion_sfx_;
  Sound boss_hit_sfx_;
  bool audio_ready_ = false;
  float score_anim_time_ = 0.0f;
};
