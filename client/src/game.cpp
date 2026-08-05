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

void Game::update(float dt) {
  if (screen_ == Screen::CONNECTING) {
    connection_timer_ += dt;
    if (connection_timer_ > 1.0f) {
      screen_ = Screen::WAITING;
    }
  }

  // animate score screen border when showing final results
  if (screen_ == Screen::GAME_OVER || screen_ == Screen::WIN) {
    score_anim_time_ += dt;
  }
}

void Game::init() {
  screen_ = Screen::MENU;

  std::array<uint8_t, MAX_PLAYERS> player_ids;
  for (uint8_t i = 0; i < MAX_PLAYERS; ++i) {
    player_ids[i] = i;
  }

  sim_ = shared::GameSim();
  sim_.start(player_ids);

  state_ = shared::GameState();

  // keep a copy of the previous state for detecting events
  prev_state_ = state_;

  for (auto &s : stars_)
    s.initRandom(SCREEN_WIDTH, SCREEN_HEIGHT);

  shared::PlayerInput input{
      .tick = 42, .buttons = shared::BUTTON_LEFT, .player_id = 0};

  nlohmann::json j = input;
  std::string wire = j.dump();

  std::printf("Serialized PlayerInput: %s\n", wire.c_str());

  auto received = nlohmann::json::parse(wire).get<shared::PlayerInput>();
  assert(received.tick == input.tick);
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

  // initialize audio device and load SFX
  InitAudioDevice();
  if (IsAudioDeviceReady()) {
    audio_ready_ = true;
    shoot_sfx_ = LoadSound("assets/shoot.wav");
    explosion_sfx_ = LoadSound("assets/explosion.wav");
    boss_hit_sfx_ = LoadSound("assets/boss_hit.wav");
  } else {
    audio_ready_ = false;
  }

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

      update(FIXED_DT);

      std::array<shared::PlayerInput, MAX_PLAYERS> inputs;
      inputs[0].buttons = getPlayerInputs();
      inputs[0].player_id = 0;

      if (screen_ == Screen::PLAYING) {
        sim_.step(state_, inputs, FIXED_DT);

        // play sounds for events (enemy death, boss hit)
        if (audio_ready_) {
          for (std::size_t idx = 0; idx < state_.enemies.size(); ++idx) {
            if (prev_state_.enemies[idx][0] == 1 && state_.enemies[idx][0] == 0) {
              PlaySound(explosion_sfx_);
            }
          }

          if (prev_state_.boss.active && state_.boss.active &&
              prev_state_.boss.health > state_.boss.health) {
            PlaySound(boss_hit_sfx_);
          }
        }

        prev_state_ = state_;
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
  if (audio_ready_) {
    UnloadSound(shoot_sfx_);
    UnloadSound(explosion_sfx_);
    UnloadSound(boss_hit_sfx_);
    CloseAudioDevice();
  }
  CloseWindow();
}

void Game::handleInput() {
  switch (screen_) {
  case Screen::MENU:
    if (IsKeyPressed(KEY_LEFT)) {
      mode_ = GameMode::LOCAL;
    } else if (IsKeyPressed(KEY_RIGHT)) {
      mode_ = GameMode::ONLINE;
    } else if (IsKeyPressed(KEY_ENTER)) {
      if (mode_ == GameMode::ONLINE) {
        screen_ = Screen::CONNECTING;
        connection_timer_ = 0.0f;
      } else {
        screen_ = Screen::PLAYING;
      }
    }
    break;

  case Screen::CONNECTING:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::MENU;
    }
    break;

  case Screen::WAITING:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::MENU;
    }
    break;

  case Screen::PLAYING:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::PAUSED;
    }
    if (IsKeyPressed(KEY_SPACE)) {
      if (audio_ready_)
        PlaySound(shoot_sfx_);
    }
    break;

  case Screen::PAUSED:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::PLAYING;
    }
    break;

  case Screen::GAME_OVER:
    if (IsKeyPressed(KEY_ENTER)) {
      init();
    }
    if (IsKeyPressed(KEY_R)) {
      init();
    }
    break;

  case Screen::WIN:
    if (IsKeyPressed(KEY_ENTER)) {
      init();
    }
    if (IsKeyPressed(KEY_R)) {
      init();
    }
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
  case Screen::MENU:
    DrawText("Use LEFT/RIGHT to choose mode", SCREEN_WIDTH / 2 - 180,
             SCREEN_HEIGHT / 2 - 40, 20, WHITE);
    DrawText("Press ENTER to confirm", SCREEN_WIDTH / 2 - 150,
             SCREEN_HEIGHT / 2 - 10, 20, WHITE);
    DrawText("Local", SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT / 2 + 40, 24,
             mode_ == GameMode::LOCAL ? YELLOW : WHITE);
    DrawText("Online", SCREEN_WIDTH / 2 + 50, SCREEN_HEIGHT / 2 + 40, 24,
             mode_ == GameMode::ONLINE ? YELLOW : WHITE);
    break;

  case Screen::CONNECTING:
    DrawText("Connecting to server...", SCREEN_WIDTH / 2 - 170,
             SCREEN_HEIGHT / 2, 24, LIGHTGRAY);
    DrawText("Press ESC to cancel", SCREEN_WIDTH / 2 - 140,
             SCREEN_HEIGHT / 2 + 40, 20, WHITE);
    break;

  case Screen::WAITING:
    DrawText("Waiting for player 2...", SCREEN_WIDTH / 2 - 170,
             SCREEN_HEIGHT / 2, 24, LIGHTGRAY);
    DrawText("Press ESC to return to menu", SCREEN_WIDTH / 2 - 170,
             SCREEN_HEIGHT / 2 + 40, 20, WHITE);
    break;

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
  {
    // Draw a centered panel with final scores and animated border
    const int boxW = 440;
    const int boxH = 200;
    const float bx = SCREEN_WIDTH / 2.0f - boxW / 2.0f;
    const float by = SCREEN_HEIGHT / 2.0f - boxH / 2.0f;
    Rectangle rec{bx, by, (float)boxW, (float)boxH};
    DrawRectangleRec(rec, Fade(BLACK, 0.7f));

    // animated border pulse
    float pulse = (sinf(score_anim_time_ * 3.0f) * 0.5f + 0.5f);
    Color borderCol = Fade(YELLOW, 0.4f + 0.6f * pulse);
    DrawRectangleLinesEx(rec, 4, borderCol);

    // Title
    const char *title = (screen_ == Screen::GAME_OVER) ? "Game Over" : "Final Score";
    DrawText(title, (int)(SCREEN_WIDTH / 2 - MeasureText(title, 32) / 2),
             (int)(by + 12), 32, WHITE);

    // Player scores
    int y = (int)(by + 60);
    int idx = 0;
    int winner_id = 0;
    uint32_t best = 0;
    for (const auto &p : state_.players) {
      // display only valid players (id < MAX_PLAYERS)
      if (p.id >= MAX_PLAYERS)
        continue;
      const char *line = TextFormat("Player %d: %u", p.id, p.points);
      Color col = (p.id == 0) ? YELLOW : LIGHTGRAY;
      DrawText(line, (int)(bx + 24), y, 20, col);
      y += 28;
      if (p.points > best) {
        best = p.points;
        winner_id = p.id;
      }
      if (++idx >= MAX_PLAYERS)
        break;
    }

    // Winner line
    const char *winLine = TextFormat("Winner: Player %d", winner_id);
    DrawText(winLine, (int)(SCREEN_WIDTH / 2 - MeasureText(winLine, 24) / 2),
             (int)(by + boxH - 60), 24, WHITE);

    // Controls
    DrawText("Press R to restart or ENTER to return to menu",
             (int)(SCREEN_WIDTH / 2 - MeasureText("Press R to restart or ENTER to return to menu", 18) / 2),
             (int)(by + boxH - 30), 18, LIGHTGRAY);
    break;
  }
  }

  DrawFPS(10, 10);
}
