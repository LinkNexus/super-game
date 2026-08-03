#pragma once

#include "raylib.h"
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

  // Particle system for explosion effects (client-side only)
  struct Particle {
    Vector2 position{};
    Vector2 velocity{};
    float lifetime = 0.0f;
    float max_lifetime = 0.0f;
    Color color{255, 255, 255, 255};
    float size = 3.0f;
  };

  void updateParticles(float dt);
  void drawParticles() const;
  void spawnExplosion(const Vector2 &pos, shared::EnemyType type);
  void spawnEnemyExplosions(const shared::GameState &before,
                            const shared::GameState &after);

  static constexpr int MAX_PARTICLES = 128;
  std::array<Particle, MAX_PARTICLES> particles_{};

  Player player_;
  Boss boss_;
  std::array<Star, STAR_COUNT> stars_;
  Screen screen_;
  shared::GameSim sim_;
  shared::GameState state_;
};
