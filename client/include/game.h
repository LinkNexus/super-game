#pragma once

#include "audio_manager.h"
#include "constants.h"
#include "elements/text_input.h"
#include "game_designer.h"
#include "raylib.h"
#include "session.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include "types.h"
#include <array>
#include <memory>
#include <string>

/// Owns the raylib window/audio lifecycle, menu/screen flow, and a
/// `Session` (local or online) that supplies each tick's `GameState`.
/// Render-only: no simulation logic lives here, only `draw()` calls and
/// input polling that gets packaged into `PlayerInput`s for the session.
class Game {
public:
  explicit Game(std::string_view server_url = default_server_url);

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
  void pollPlayersInputs();

  void spawnExplosion(const Vector2 &pos, shared::EnemyType type);

  /// Diffs @p before and @p after to spawn explosion particles for enemies
  /// that died this tick, since the wire `GameState` doesn't carry death
  /// events itself.
  void spawnEnemyExplosions(const shared::CoopGameState &before,
                            const shared::CoopGameState &after);

  void handleEventsOnScreen();

  void checkAndPlaySoundsOnEvents();

  void startLocalSession();
  void startOnlineSession();

  void initNameTextBoxes();

private:
  GameDesigner game_designer_{};
  AudioManager audio_manager_{};

  static constexpr std::array<
      std::array<std::pair<KeyboardKey, shared::Button>, 3>,
      MAX_PLAYERS_ON_THIS_MACHINE>
      PLAYERS_CONTROLS{{{{
                            {KEY_LEFT, shared::Button::BUTTON_LEFT},
                            {KEY_RIGHT, shared::Button::BUTTON_RIGHT},
                            {KEY_SPACE, shared::Button::BUTTON_SHOOT},
                        }},
                        {{
                            {KEY_A, shared::Button::BUTTON_LEFT},
                            {KEY_D, shared::Button::BUTTON_RIGHT},
                            {KEY_W, shared::Button::BUTTON_SHOOT},
                        }}}};

  std::array<Particle, MAX_PARTICLES> particles_{};
  std::array<Star, STAR_COUNT> stars_;

  GameScreen screen_{GameScreen::MENU};
  GameType type_{GameType::LOCAL};

  std::unique_ptr<Config> config_{nullptr};
  std::unique_ptr<Session> session_{nullptr};

  std::string server_url_{};

  /// Per-slot local input state, indexed by local player slot (not
  /// necessarily the sim's player id) and sent to the active `Session`
  /// each tick.
  std::array<shared::PlayerInput, MAX_PLAYERS_ON_THIS_MACHINE> inputs_{};
  std::array<TextInput, shared::MAX_PLAYERS_PER_CLIENT> name_text_boxes_{};

  float score_anim_time_{};
};
