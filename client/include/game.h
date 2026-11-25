#pragma once

#include "audio_manager.h"
#include "constants.h"
#include "elements/text_input.h"
#include "game_designer.h"
#include "raylib.h"
#include "session.h"
#include "shared/sim/constants.h"
#include "shared/sim/messages.h"
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
  explicit Game(std::string_view server_url = DEFAULT_SERVER_URL);

  explicit Game(std::array<Star, STAR_COUNT> stars) : stars_(stars) {}

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

  /// Per local player: ready toggle and, in PvP, team switch. Reads the
  /// current state from the last lobby update, never from a local copy.
  void handleLobbyInput(OnlineSession &session);

  /// Polls raylib input and writes the resulting buttons into `inputs_`
  /// for whichever local player(s) are active this tick.
  ThisMachinePlayerInputs pollPlayersInputs();

  void spawnExplosion(const Vector2 &pos, shared::sim::EnemyType type);

  /// Diffs @p before and @p after to spawn explosion particles for enemies
  /// that died this tick, since the wire `GameState` doesn't carry death
  /// events itself.
  void spawnEnemyExplosions(
      const shared::sim::CoopGameState &before, const shared::sim::CoopGameState &after
  );

  void handleEventsOnScreen();

  void checkAndPlaySoundsOnEvents();

  void startLocalSession();
  void startOnlineSession();

  void initNameTextBoxes();

private:
  GameDesigner game_designer_{};
  AudioManager audio_manager_{};

  static constexpr std::
      array<std::pair<shared::sim::Button, std::array<KeyboardKey, MAX_PLAYERS_ON_THIS_MACHINE>>, 3>
          PLAYERS_CONTROLS = {
              {{shared::sim::Button::LEFT, {KEY_LEFT, KEY_A}},
               {shared::sim::Button::RIGHT, {KEY_RIGHT, KEY_D}},
               {shared::sim::Button::SHOOT, {KEY_SPACE, KEY_W}}}
  };

  // static constexpr std::
  //     array<std::array<std::pair<KeyboardKey, shared::Button>, 3>, MAX_PLAYERS_ON_THIS_MACHINE>
  //         PLAYERS_CONTROLS{
  //             {{{
  //                  {KEY_LEFT, shared::sim::Button::LEFT},
  //                  {KEY_RIGHT, shared::sim::Button::RIGHT},
  //                  {KEY_SPACE, shared::sim::Button::SHOOT},
  //              }},
  //              {{
  //                  {KEY_A, shared::sim::Button::LEFT},
  //                  {KEY_D, shared::sim::Button::RIGHT},
  //                  {KEY_W, shared::sim::Button::SHOOT},
  //              }}}
  //         };

  /// The lobby reuses each player's controls: shoot readies, left/right
  /// moves to the neighbouring team. Keep in sync with PLAYERS_CONTROLS.
  static constexpr LobbyKeyHints PLAYERS_LOBBY_KEY_HINTS{{
      {"SPACE", "LEFT/RIGHT"},
      {"W", "A/D"},
  }};

  std::array<Particle, MAX_PARTICLES> particles_{};
  std::array<Star, STAR_COUNT> stars_;

  GameScreen screen_{GameScreen::MENU};
  GameType type_{GameType::LOCAL};

  std::unique_ptr<Config> config_{nullptr};
  std::unique_ptr<Session> session_{nullptr};

  std::string server_url_{};

  std::array<TextInput, shared::sim::MAX_PLAYERS_PER_CLIENT> name_text_boxes_{};

  float score_anim_time_{};
};
