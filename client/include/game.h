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

enum class Screen {
  MENU,
  CONNECTING,
  LOBBY,
  SELECT_LOCAL_MODE,
  PLAYING,
  PAUSED,
  GAME_OVER,
  WIN
};

enum class GameMode { LOCAL, ONLINE };

class Game {
public:
  explicit Game(std::string server_url = shared::default_server_url);
  void run();

private:
  void init();
  void draw();
  void handleInput();
  void getPlayersInputs();

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
  std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS> inputs_{};
  std::optional<LocalMode> selected_local_mode_{};
  std::unique_ptr<Session> session_ = nullptr;
  shared::GameState state_;
  shared::GameState prev_state_;

  // audio
  Sound shoot_sfx_;
  Sound explosion_sfx_;
  Sound boss_hit_sfx_;
  Music background_music_;
  bool music_loaded_ = false;
  bool audio_ready_ = false;
  float music_volume_ = 0.0f;
  float target_music_volume_ = 0.0f;
  static constexpr float MUSIC_FADE_SPEED = 1.0f; // volume units per second

  float score_anim_time_ = 0.0f;
  float status_text_offset_y_ = 10.0f;
  std::string server_url_;
};
