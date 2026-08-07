#pragma once

#include "constants.h"
#include "entities/boss.h"
#include "entities/player.h"
#include "entities/star.h"
#include "raylib.h"
#include "session.h"
#include "shared/messages.h"
#include <array>
#include <string>

enum class Screen { MENU, CONNECTING, LOBBY, PLAYING, PAUSED, GAME_OVER, WIN };

enum class GameMode { LOCAL, ONLINE };

class Game {
public:
  explicit Game(std::string server_url = shared::default_server_url);
  void run();

private:
  void init();
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
  GameMode mode_ = GameMode::LOCAL;
  std::unique_ptr<Session> session_ = nullptr;
  shared::GameState state_;
  float status_text_offset_y_ = 10.0f;
  std::string server_url_;
};
