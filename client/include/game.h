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

/// Client-only UI/flow state. Governs *whether* `Game` steps the
/// simulation at all - `shared::GamePhase` (sim-side progression) is a
/// separate concern that rides along in `GameState::phase`. Pause has no
/// sim-side representation: `Game::run()` simply skips stepping the
/// session while `PAUSED` and keeps drawing the last known state.
enum class Screen {
  MENU,
  NAME_ENTRY,
  CONNECTING,
  LOBBY,
  SELECT_LOCAL_MODE,
  PLAYING,
  PAUSED,
  GAME_OVER,
  WIN
};

/// Which kind of `Session` the player picked from the main menu.
enum class GameMode { LOCAL, ONLINE };

/// Owns the raylib window/audio lifecycle, menu/screen flow, and a
/// `Session` (local or online) that supplies each tick's `GameState`.
/// Render-only: no simulation logic lives here, only `draw()` calls and
/// input polling that gets packaged into `PlayerInput`s for the session.
class Game {
public:
  explicit Game(std::string server_url = shared::default_server_url);

  /// Runs the full window/game loop until the window is closed: polls
  /// input, steps the active session at a fixed timestep, and renders.
  void run();

private:
  /// Resets all screen/session state back to `Screen::MENU` (used on
  /// startup and on returning to the menu from `GAME_OVER`/`WIN`).
  void init();

  /// Restarts a match directly, bypassing the menu: recreates a
  /// `LocalSession` with the previously-selected mode, or reconnects a
  /// fresh `OnlineSession`, depending on `mode_`.
  void restart();

  void draw();
  void handleInput();

  /// Polls raylib input and writes the resulting buttons into `inputs_`
  /// for whichever local player(s) are active this tick.
  void getPlayersInputs();

  void drawInputTextBox() const;
  void drawLobby() const;

  /// Particle system for explosion effects (client-side only).
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

  /// Diffs @p before and @p after to spawn explosion particles for enemies
  /// that died this tick, since the wire `GameState` doesn't carry death
  /// events itself.
  void spawnEnemyExplosions(const shared::GameState &before,
                            const shared::GameState &after);

  /// Creates a fresh `LocalSession` for @p mode and resets `inputs_`
  /// (player ids, dual-player slot) to match it.
  void startLocalSession(LocalMode mode);

  /// Creates a fresh `OnlineSession` against `server_url_` (with the
  /// chosen `player_name_` as a query parameter) and moves to
  /// `Screen::CONNECTING`.
  void startOnlineSession();

  static constexpr int MAX_PARTICLES = 128;
  std::array<Particle, MAX_PARTICLES> particles_{};

  Player player_;
  Boss boss_;
  std::array<Star, STAR_COUNT> stars_;
  Screen screen_;
  GameMode mode_ = GameMode::LOCAL;
  /// Per-slot local input state, indexed by local player slot (not
  /// necessarily the sim's player id) and sent to the active `Session`
  /// each tick.
  std::array<std::optional<shared::PlayerInput>, shared::MAX_PLAYERS> inputs_{};
  std::optional<LocalMode> selected_local_mode_{};
  std::unique_ptr<Session> session_ = nullptr;
  shared::GameState state_;
  /// Previous tick's state, kept for edge-detection (enemy deaths, boss
  /// hits) that the current `GameState` snapshot alone can't reveal.
  shared::GameState prev_state_;

  int frames_counter{};
  bool mouse_on_name_input_text{false};
  Rectangle name_text_box{shared::SCREEN_WIDTH / 2.0f - 130.0f,
                          shared::SCREEN_HEIGHT / 2.0f - 25.0f, 250.0f, 50.0f};
  char player_name_[shared::MAX_NAME_LENGTH + 1]{};
  int name_letters_count_{};
  bool showPlayerNameError_{};

  /// Local mirror of this client's own ready state, toggled by SPACE on
  /// `Screen::LOBBY` and sent to the server via `OnlineSession::sendReady`.
  bool lobby_am_i_ready{};

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

  std::string server_url_;
};
