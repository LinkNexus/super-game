#pragma once

#include "raylib.h"

enum class GameType { LOCAL, ONLINE };

/// Client-only UI/flow state. Governs *whether* `Game` steps the
/// simulation at all - `shared::GamePhase` (sim-side progression) is a
/// separate concern that rides along in `GameState::phase`. Pause has no
/// sim-side representation: `Game::run()` simply skips stepping the
/// session while `PAUSED` and keeps drawing the last known state.
enum class GameScreen {
  MENU,
  SELECT_MODE,
  SELECT_PLAYER_COUNT,
  SELECT_PLAYERS_COUNT_ON_THIS_MACHINE,
  NAME_ENTRY,
  CONNECTING,
  LOBBY,
  PLAYING,
  PAUSED,
  END
};

/// Particle system for explosion effects (client-side only).
struct Particle {
  Vector2 position{};
  Vector2 velocity{};
  float lifetime = 0.0f;
  float max_lifetime = 0.0f;
  Color color{255, 255, 255, 255};
  float size = 3.0f;

  void update(float dt);
};

/// Purely decorative parallax-background star, client-only (no equivalent
/// in the shared simulation).
struct Star {
  Vector2 position;
  float speed;
  float size;
  Color color;

  /// Places this star at a random position/speed/size within the screen
  /// bounds.
  void initRandom();

  /// Moves the star downward and wraps it back to the top once it leaves
  /// the screen.
  void update(float dt);
};
