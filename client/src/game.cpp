#include "game.h"
#include "constants.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/star.h"
#include "raylib.h"
#include "shared/messages.h"
#include "shared/sim/enemy_sim.h"
#include <cfloat>
#include <cstdlib>

void Game::init() {
  screen_ = Screen::MENU;

  std::array<uint8_t, MAX_PLAYERS> player_ids;
  player_ids.fill(0);

  state_ = shared::GameState();

  for (auto &s : stars_)
    s.initRandom(SCREEN_WIDTH, SCREEN_HEIGHT);
}

shared::Button Game::getPlayerInputs() {
  shared::Button buttons = shared::BUTTON_NONE;

  if (IsKeyDown(KEY_LEFT))
    buttons = static_cast<shared::Button>(buttons | shared::BUTTON_LEFT);
  if (IsKeyDown(KEY_RIGHT))
    buttons = static_cast<shared::Button>(buttons | shared::BUTTON_RIGHT);
  if (IsKeyDown(KEY_SPACE))
    buttons = static_cast<shared::Button>(buttons | shared::BUTTON_SHOOT);

  return buttons;
}

void Game::run() {
  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "SUPER GAME");
  ChangeDirectory(GetApplicationDirectory());
  SetExitKey(KEY_NULL);
  SetTargetFPS(TARGET_FPS);

  init();

  player_.loadTexture();
  Enemy::loadTextures();
  boss_.loadTexture();

  float accumulator = 0.0f;

  while (!WindowShouldClose()) {
    handleInput();

    float frame_time = GetFrameTime();
    if (frame_time > 0.1f)
      frame_time = 0.1f;
    accumulator += frame_time;

    while (accumulator >= FIXED_DT) {
      for (auto &s : stars_)
        s.update(FIXED_DT, SCREEN_HEIGHT, SCREEN_WIDTH);

      std::array<shared::PlayerInput, MAX_PLAYERS> inputs;
      inputs[0].buttons = getPlayerInputs();
      inputs[0].player_id = 0;

      if (screen_ == Screen::PLAYING) {
        state_ = session_->step(inputs[0], FIXED_DT);
      }

      if (screen_ != Screen::GAME_OVER && screen_ != Screen::WIN) {
        auto phase = static_cast<shared::GamePhase>(state_.phase);
        if (phase == shared::GamePhase::GAME_OVER)
          screen_ = Screen::GAME_OVER;
        if (phase == shared::GamePhase::WON)
          screen_ = Screen::WIN;
      }

      accumulator -= FIXED_DT;
    }

    BeginDrawing();
    ClearBackground(BLACK);
    draw();
    EndDrawing();
  }

  player_.unload();
  Enemy::unloadTextures();
  boss_.unload();
  CloseWindow();
}

void Game::handleInput() {
  switch (screen_) {
  case Screen::MENU:
    if (IsKeyPressed(KEY_L)) {
      session_ = std::make_unique<LocalSession>();
      screen_ = Screen::PLAYING;
    }

    else if (IsKeyPressed(KEY_O)) {
      session_ = std::make_unique<OnlineSession>("ws://localhost:9001");
      screen_ = Screen::LOBBY;
    }

    break;

  case Screen::LOBBY:
    if (IsKeyPressed(KEY_ESCAPE))
      screen_ = Screen::MENU;

    break;

  case Screen::PLAYING:
    if (IsKeyPressed(KEY_ESCAPE))
      screen_ = Screen::PAUSED;
    break;

  case Screen::PAUSED:
    if (IsKeyPressed(KEY_ESCAPE))
      screen_ = Screen::PLAYING;

    break;

  case Screen::GAME_OVER:
    if (IsKeyPressed(KEY_ENTER))
      init();

    break;

  case Screen::WIN:
    if (IsKeyPressed(KEY_ENTER))
      init();

    break;
  }
}

void drawBullet(const shared::BulletState &state) {
  if (!state.active)
    return;
  DrawRectangle((int)state.position.x - shared::BulletSimState::WIDTH / 2,
                (int)state.position.y - shared::BulletSimState::HEIGHT / 2,
                shared::BulletSimState::WIDTH, shared::BulletSimState::HEIGHT,
                YELLOW);
}

void Game::draw() {
  for (const auto &s : stars_)
    s.draw();

  switch (screen_) {
  case Screen::MENU: {
    const auto text = "Press L to play locally or O to start an online session";
    DrawText(text, (SCREEN_WIDTH - MeasureText(text, 20)) / 2,
             SCREEN_HEIGHT / 2, 20, WHITE);
    break;
  }

  case Screen::LOBBY: {
    const auto text = "Waiting for players to join...";
    DrawText(text, (SCREEN_WIDTH - MeasureText(text, 20)) / 2,
             SCREEN_HEIGHT / 2, 20, WHITE);
    break;
  }

  case Screen::PLAYING:
  case Screen::PAUSED: {
    auto player_state =
        std::find_if(state_.players.begin(), state_.players.end(),
                     [](auto &p) { return p.id == 0; });

    if (player_state != state_.players.end())
      player_.draw(*player_state);

    for (const auto &b : state_.bullets)
      drawBullet(b);

    for (std::size_t idx = 0; idx < state_.enemies.size(); ++idx) {
      if (state_.enemies[idx][0] == 1) {
        float pos_x = state_.enemies_offset_x +
                      (idx % shared::EnemiesPoolSimState::COLS) *
                          (shared::EnemySimState::WIDTH +
                           shared::EnemiesPoolSimState::SPACING_X);
        float pos_y = state_.enemies_offset_y +
                      (idx / shared::EnemiesPoolSimState::COLS) *
                          (shared::EnemySimState::HEIGHT +
                           shared::EnemiesPoolSimState::SPACING_Y);
        Enemy::draw(pos_x, pos_y,
                    static_cast<shared::EnemyType>(state_.enemies[idx][1]));
      }
    }

    boss_.draw(state_.boss, static_cast<shared::GamePhase>(state_.phase) ==
                                shared::GamePhase::FIGHT_BOSS);

    if (screen_ == Screen::PAUSED) {
      DrawRectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, Fade(BLACK, 0.6f));
      DrawText("Game Paused", SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2, 20,
               WHITE);
      DrawText("Press ESC to resume", SCREEN_WIDTH / 2 - 100,
               SCREEN_HEIGHT / 2 + 30, 20, WHITE);
    }
    break;
  }

  case Screen::GAME_OVER:
  case Screen::WIN:
    DrawText(screen_ == Screen::GAME_OVER ? "Game Over" : "You Win!",
             SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2, 20, WHITE);
    DrawText("Press ENTER to return to menu", SCREEN_WIDTH / 2 - 150,
             SCREEN_HEIGHT / 2 + 30, 20, WHITE);

    break;
  }

  DrawFPS(10, 10);
}
