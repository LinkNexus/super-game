#include "game.h"
#include "constants.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/star.h"
#include "raylib.h"
#include "session.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/game_sim.h"
#include "utils.h"
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <string>

float getCenteredTextX(const char *text, int font_size) {
  return (shared::SCREEN_WIDTH - MeasureText(text, font_size)) / 2.0f;
}

Game::Game(std::string server_url) {
  online_config_.server_url = std::move(server_url);
}

void Game::restart() {
  if (mode_ == GameMode::LOCAL) {
    startLocalSession();
  } else {
    startOnlineSession();
  }
}

void Game::init() {
  screen_ = Screen::MENU;

  // (session-based play) session is created when starting a game
  state_ = shared::GameState();
  inputs_[1] = std::nullopt;

  // keep a copy of the previous state for detecting events
  prev_state_ = state_;

  for (auto &s : stars_)
    s.initRandom(shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT);
}

void Game::getPlayersInputs() {
  shared::Button mainPlayerButtons = shared::BUTTON_NONE;

  if (IsKeyDown(KEY_LEFT))
    mainPlayerButtons =
        static_cast<shared::Button>(mainPlayerButtons | shared::BUTTON_LEFT);
  if (IsKeyDown(KEY_RIGHT))
    mainPlayerButtons =
        static_cast<shared::Button>(mainPlayerButtons | shared::BUTTON_RIGHT);
  if (IsKeyDown(KEY_SPACE))
    mainPlayerButtons =
        static_cast<shared::Button>(mainPlayerButtons | shared::BUTTON_SHOOT);

  inputs_[0]->buttons = mainPlayerButtons;

  if (LocalSession *s = dynamic_cast<LocalSession *>(session_.get())) {
    if (s->getMode() != LocalSession::Mode::SINGLE_PLAYER) {
      auto secondPlayerButtons = shared::BUTTON_NONE;

      if (IsKeyDown(KEY_A))
        secondPlayerButtons = static_cast<shared::Button>(secondPlayerButtons |
                                                          shared::BUTTON_LEFT);
      if (IsKeyDown(KEY_D))
        secondPlayerButtons = static_cast<shared::Button>(secondPlayerButtons |
                                                          shared::BUTTON_RIGHT);
      if (IsKeyDown(KEY_W))
        secondPlayerButtons = static_cast<shared::Button>(secondPlayerButtons |
                                                          shared::BUTTON_SHOOT);

      inputs_[1]->buttons = secondPlayerButtons;
    }
  }
}

void Game::run() {
  InitWindow(shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT, "SUPER GAME");
  ChangeDirectory(GetApplicationDirectory());
  SetExitKey(KEY_NULL);
  SetTargetFPS(TARGET_FPS);

  init();

  // initialize audio device and load SFX + music
  InitAudioDevice();
  if (IsAudioDeviceReady()) {
    audio_ready_ = true;
    shoot_sfx_ = LoadSound("assets/shoot.wav");
    explosion_sfx_ = LoadSound("assets/explosion.ogg");
    boss_hit_sfx_ = LoadSound("assets/boss_hit.ogg");
    background_music_ = LoadMusicStream("assets/backgroundsound.ogg");
    if (IsMusicValid(background_music_)) {
      music_loaded_ = true;
      target_music_volume_ = 0.0f;
      music_volume_ = 0.0f;
      SetMusicVolume(background_music_, 0.0f);
      PlayMusicStream(background_music_);
    }
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

    while (accumulator >= shared::FIXED_DT) {
      for (auto &s : stars_)
        s.update(shared::FIXED_DT, shared::SCREEN_HEIGHT, shared::SCREEN_WIDTH);

      if (audio_ready_ && music_loaded_) {
        if (screen_ == Screen::PLAYING)
          target_music_volume_ = 0.25f;
        else
          target_music_volume_ = 0.7f;

        if (music_volume_ < target_music_volume_) {
          music_volume_ =
              std::min(music_volume_ + MUSIC_FADE_SPEED * shared::FIXED_DT,
                       target_music_volume_);
        } else if (music_volume_ > target_music_volume_) {
          music_volume_ =
              std::max(music_volume_ - MUSIC_FADE_SPEED * shared::FIXED_DT,
                       target_music_volume_);
        }
        SetMusicVolume(background_music_, music_volume_);
      }

      if (screen_ == Screen::CONNECTING) {
        if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
          if (s->getPlayerId() != 0) {
            screen_ = Screen::LOBBY;
          }
        }
      }

      if (screen_ == Screen::LOBBY) {
        if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
          std::visit(shared::overloaded{[this](const auto &u) {
                       if (u.game_started)
                         screen_ = Screen::PLAYING;
                     }},
                     s->getLobbyUpdate());
        }
      }

      if (screen_ == Screen::PLAYING) {
        getPlayersInputs();
        if (session_)
          state_ = session_->step(inputs_, shared::FIXED_DT);

        // play sounds for events (enemy death, boss hit)
        if (audio_ready_) {
          std::visit(
              shared::overloaded{
                  [&](shared::CoopGameState &s, shared::CoopGameState &prev_s) {
                    for (std::size_t idx = 0; idx < s.enemies.size(); ++idx) {
                      if (prev_s.enemies[idx][0] == 1 &&
                          s.enemies[idx][0] == 0) {
                        PlaySound(explosion_sfx_);
                      }
                    }

                    if (prev_s.boss.active && s.boss.active &&
                        prev_s.boss.health > s.boss.health) {
                      PlaySound(boss_hit_sfx_);
                    }

                    spawnEnemyExplosions(prev_s, s);
                  },
                  [](shared::PvPGameState &s, shared::PvPGameState &prev_s) {

                  },
                  [](auto &, auto &) {}},
              state_, prev_state_);
        }

        prev_state_ = state_;
      }

      if (screen_ != Screen::END) {
        std::visit(
            shared::overloaded{[&](shared::CoopGameState &s) {
                                 using GamePhase = shared::CoopGameSim::Phase;
                                 auto phase = static_cast<GamePhase>(s.phase);

                                 if (phase == GamePhase::GAME_OVER ||
                                     phase == GamePhase::WON) {
                                   session_ = nullptr;
                                   screen_ = Screen::END;
                                 }
                               },
                               [&](shared::PvPGameState &s) {
                                 using GamePhase = shared::PvPGameSim::Phase;
                                 auto phase = static_cast<GamePhase>(s.phase);

                                 if (phase == GamePhase::END) {
                                   session_ = nullptr;
                                   screen_ = Screen::END;
                                 }
                               }},
            state_);
      } else {
        // animate score screen border while showing final results
        score_anim_time_ += shared::FIXED_DT;
      }

      accumulator -= shared::FIXED_DT;
    }

    // Update particles using the real frame time so they feel smooth
    updateParticles(frame_time);

    if (audio_ready_ && music_loaded_) {
      UpdateMusicStream(background_music_);
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
    if (music_loaded_)
      UnloadMusicStream(background_music_);
    CloseAudioDevice();
  }
  CloseWindow();
}

void Game::startLocalSession() {
  auto selectedMode = local_config_.modes[local_config_.mode_idx];
  session_ = std::make_unique<LocalSession>(selectedMode);
  screen_ = Screen::PLAYING;

  inputs_[0].emplace();
  inputs_[0]->player_id = 1;

  if (selectedMode != LocalSession::Mode::SINGLE_PLAYER) {
    inputs_[1].emplace();
    inputs_[1]->player_id = 2;
  }

  state_ = shared::GameState();
  prev_state_ = state_;
}

bool isNameCharValid(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void Game::startOnlineSession() {
  inputs_[0].emplace();

  std::string url =
      online_config_.server_url + "?name=" + online_config_.player_name;
  auto selectedOnlineMode = online_config_.modes[online_config_.mode_idx];

  if (selectedOnlineMode != OnlineMode::COOP) {
    url += "&players=" +
           std::to_string(selectedOnlineMode == OnlineMode::_1V1 ? 1 : 2);
  }

  session_ = std::make_unique<OnlineSession>(url);
  screen_ = Screen::CONNECTING;
  state_ = shared::GameState();
  prev_state_ = state_;
  online_config_.is_ready = false;
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
        screen_ = Screen::SELECT_ONLINE_MODE;
      } else {
        screen_ = Screen::SELECT_LOCAL_MODE;
      }
    }
    break;

  case Screen::SELECT_ONLINE_MODE:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::MENU;
    } else if (IsKeyPressed(KEY_LEFT)) {
      if (online_config_.mode_idx > 0)
        online_config_.mode_idx--;
      else
        online_config_.mode_idx = online_config_.modes.size() - 1;
    } else if (IsKeyPressed(KEY_RIGHT)) {
      if (online_config_.mode_idx < online_config_.modes.size() - 1)
        online_config_.mode_idx++;
      else
        online_config_.mode_idx = 0;
    } else if (IsKeyPressed(KEY_ENTER))
      screen_ = Screen::NAME_ENTRY;

    break;

  case Screen::NAME_ENTRY: {
    int key = GetCharPressed();

    while (key > 0) {
      showPlayerNameError_ = false;

      if (isNameCharValid((char)key) &&
          name_letters_count_ < shared::MAX_NAME_LENGTH) {
        online_config_.player_name[name_letters_count_] = (char)key;
        online_config_.player_name[name_letters_count_ + 1] = '\0';
        name_letters_count_++;
      }

      key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
      name_letters_count_--;
      if (name_letters_count_ < 0)
        name_letters_count_ = 0;
      online_config_.player_name[name_letters_count_] = '\0';
    }

    if (IsKeyPressed(KEY_ENTER)) {
      if (name_letters_count_ == 0) {
        showPlayerNameError_ = true;
        break;
      }
      startOnlineSession();
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      name_letters_count_ = 0;
      online_config_.player_name[name_letters_count_] = '\0';
      screen_ = Screen::MENU;
    }

    break;
  }

  case Screen::CONNECTING:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::MENU;
    }
    break;

  case Screen::LOBBY:
    if (IsKeyPressed(KEY_ESCAPE)) {
      session_ = nullptr;
      screen_ = Screen::MENU;
    }

    if (IsKeyPressed(KEY_SPACE)) {
      if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
        online_config_.is_ready = !online_config_.is_ready;
        s->sendReady(online_config_.is_ready);
      }
    }

    break;

  case Screen::SELECT_LOCAL_MODE:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::MENU;
    } else if (IsKeyPressed(KEY_LEFT)) {
      if (local_config_.mode_idx > 0)
        local_config_.mode_idx--;
      else
        local_config_.mode_idx = local_config_.modes.size() - 1;
    } else if (IsKeyPressed(KEY_RIGHT)) {
      if (local_config_.mode_idx < local_config_.modes.size() - 1)
        local_config_.mode_idx++;
      else
        local_config_.mode_idx = 0;
    } else if (IsKeyPressed(KEY_ENTER))
      startLocalSession();

    break;

  case Screen::PLAYING:
    if (IsKeyPressed(KEY_ESCAPE))
      screen_ = Screen::PAUSED;
    if (IsKeyPressed(KEY_SPACE)) {
      if (audio_ready_)
        PlaySound(shoot_sfx_);
    }
    break;

  case Screen::PAUSED:
    if (IsKeyPressed(KEY_ESCAPE))
      screen_ = Screen::PLAYING;

    break;

  case Screen::END:
    if (IsKeyPressed(KEY_ENTER))
      init();
    if (IsKeyPressed(KEY_R))
      restart();

    break;
  }
}

void Game::draw() const {
  for (const auto &s : stars_)
    s.draw();

  switch (screen_) {
  case Screen::MENU:
    drawMainMenu();
    break;

  case Screen::SELECT_ONLINE_MODE:
    drawOnlineModeSelection();
    break;

  case Screen::NAME_ENTRY:
    drawInputTextBox();
    break;

  case Screen::CONNECTING:
    DrawText("Connecting to server...", shared::SCREEN_WIDTH / 2 - 170,
             shared::SCREEN_HEIGHT / 2, 24, LIGHTGRAY);
    DrawText("Press ESC to cancel", shared::SCREEN_WIDTH / 2 - 140,
             shared::SCREEN_HEIGHT / 2 + 40, 20, WHITE);
    break;

  case Screen::LOBBY:
    drawLobby();
    break;

  case Screen::SELECT_LOCAL_MODE:
    drawLocalModeSelection();
    break;

  case Screen::PLAYING:
  case Screen::PAUSED:
    drawGame();
    break;

  case Screen::END:
    drawEndScreen();
    break;
  }

  DrawFPS(10, 10);
}

void Game::updateParticles(float dt) {
  for (auto &p : particles_) {
    if (p.lifetime <= 0.0f)
      continue;

    p.position.x += p.velocity.x * dt;
    p.position.y += p.velocity.y * dt;

    // simple damping and slight gravity
    p.velocity.x *= 0.98f;
    p.velocity.y *= 0.98f;
    p.velocity.y += 20.0f * dt;

    p.lifetime -= dt;
    if (p.lifetime < 0.0f)
      p.lifetime = 0.0f;
  }
}

void drawBullet(const shared::BulletState &state) {
  if (!state.active)
    return;

  auto screenPosition = toScreen(state.position);

  DrawRectangle((int)screenPosition.x - shared::BulletSimState::WIDTH / 2,
                (int)screenPosition.y - shared::BulletSimState::HEIGHT / 2,
                shared::BulletSimState::WIDTH, shared::BulletSimState::HEIGHT,
                YELLOW);
}

void Game::drawParticles() const {
  for (const auto &p : particles_) {
    if (p.lifetime <= 0.0f)
      continue;

    float ratio = p.lifetime / p.max_lifetime;
    if (ratio < 0.0f)
      ratio = 0.0f;
    if (ratio > 1.0f)
      ratio = 1.0f;

    Color c = p.color;
    c.a = static_cast<unsigned char>(255.0f * ratio);

    if (p.size <= 3.0f) {
      DrawCircleV(p.position, p.size, c);
    } else {
      DrawRectangleV(
          {p.position.x - p.size / 2.0f, p.position.y - p.size / 2.0f},
          {p.size, p.size}, c);
    }
  }
}

void Game::spawnExplosion(const Vector2 &pos, shared::EnemyType type) {
  constexpr float kPI = 3.14159265358979323846f;
  int count = 8 + GetRandomValue(0, 4); // 8..12 particles

  for (int i = 0; i < count; ++i) {
    // find a free particle slot
    for (auto &p : particles_) {
      if (p.lifetime > 0.0f)
        continue;

      float angle = GetRandomValue(0, 360) * (kPI / 180.0f);
      float speed = static_cast<float>(GetRandomValue(40, 200));

      p.position = pos;
      p.velocity = {std::cos(angle) * speed, std::sin(angle) * speed};
      p.lifetime = 0.35f + GetRandomValue(0, 50) / 100.0f; // 0.35 - 0.85s
      p.max_lifetime = p.lifetime;
      p.size = static_cast<float>(GetRandomValue(2, 6));

      switch (type) {
      case shared::EnemyType::TYPE_1:
        p.color = ORANGE;
        break;
      case shared::EnemyType::TYPE_2:
        p.color = PURPLE;
        break;
      default:
        p.color = GOLD;
        break;
      }

      break; // next particle
    }
  }
}

void Game::spawnEnemyExplosions(const shared::CoopGameState &before,
                                const shared::CoopGameState &after) {
  for (std::size_t idx = 0; idx < after.enemies.size(); ++idx) {
    bool was_alive = before.enemies[idx][0] != 0;
    bool is_alive = after.enemies[idx][0] != 0;

    if (was_alive && !is_alive) {
      float pos_x =
          after.enemies_offset_x + (idx % shared::EnemiesPoolSimState::COLS) *
                                       (shared::EnemySimState::WIDTH +
                                        shared::EnemiesPoolSimState::SPACING_X);
      float pos_y =
          after.enemies_offset_y - (idx / shared::EnemiesPoolSimState::COLS) *
                                       (shared::EnemySimState::HEIGHT +
                                        shared::EnemiesPoolSimState::SPACING_Y);

      spawnExplosion(toRaylibVec({pos_x, pos_y}),
                     static_cast<shared::EnemyType>(after.enemies[idx][1]));
    }
  }
}

void Game::drawMainMenu() const {
  const std::string title = "THE SUPER GAME";
  const int title_size = 64;
  const int title_x =
      (shared::SCREEN_WIDTH - MeasureText(title.c_str(), title_size)) / 2;
  DrawText(title.c_str(), title_x, 120, title_size, YELLOW);

  const std::string subtitle = "Press ENTER to play";
  const int subtitle_x =
      (shared::SCREEN_WIDTH - MeasureText(subtitle.c_str(), 24)) / 2;
  DrawText(subtitle.c_str(), subtitle_x, 220, 24, WHITE);

  DrawText("Mode selection:", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 - 40, 20, LIGHTGRAY);
  DrawText("Local", shared::SCREEN_WIDTH / 2 - 150,
           shared::SCREEN_HEIGHT / 2 + 40, 28,
           mode_ == GameMode::LOCAL ? YELLOW : WHITE);
  DrawText("Online", shared::SCREEN_WIDTH / 2 + 50,
           shared::SCREEN_HEIGHT / 2 + 40, 28,
           mode_ == GameMode::ONLINE ? YELLOW : WHITE);

  DrawText("Controls:", shared::SCREEN_WIDTH / 2 - 210,
           shared::SCREEN_HEIGHT / 2 + 120, 22, LIGHTGRAY);
  DrawText("- LEFT / RIGHT: move", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 + 150, 20, WHITE);
  DrawText("- SPACE: shoot", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 + 180, 20, WHITE);
  DrawText("- A / D / W: second player (dual)", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 + 210, 20, WHITE);
  DrawText("- ESC: cancel / pause", shared::SCREEN_WIDTH / 2 - 180,
           shared::SCREEN_HEIGHT / 2 + 240, 20, WHITE);
}

void Game::drawLocalModeSelection() const {
  const std::string instructions_text =
      "Prefer single player, dual player on the same keyboard or PvP?";
  DrawText(instructions_text.c_str(),
           (shared::SCREEN_WIDTH - MeasureText(instructions_text.c_str(), 20)) /
               2,
           shared::SCREEN_HEIGHT / 2 - 40, 20, WHITE);

  const std::string confirm_text =
      "Use LEFT/RIGHT to choose and ENTER to confirm";
  const auto confirm_text_width = MeasureText(confirm_text.c_str(), 20);
  const auto confirm_text_offset_x =
      (shared::SCREEN_WIDTH - confirm_text_width) / 2;
  DrawText(confirm_text.c_str(), confirm_text_offset_x,
           shared::SCREEN_HEIGHT / 2 - 10, 20, WHITE);

  const std::string single_player_text = "Single";
  const std::string dual_player_text = "Dual";
  const std::string pvp_text = "PvP";
  DrawText(single_player_text.c_str(), confirm_text_offset_x,
           shared::SCREEN_HEIGHT / 2 + 40, 24,
           local_config_.mode_idx == 0 ? YELLOW : WHITE);

  DrawText(dual_player_text.c_str(),
           confirm_text_offset_x + confirm_text_width / 2 -
               MeasureText(dual_player_text.c_str(), 24) / 2,
           shared::SCREEN_HEIGHT / 2 + 40, 24,
           local_config_.mode_idx == 1 ? YELLOW : WHITE);

  DrawText(pvp_text.c_str(),
           confirm_text_offset_x + confirm_text_width -
               MeasureText(pvp_text.c_str(), 24),
           shared::SCREEN_HEIGHT / 2 + 40, 24,
           local_config_.mode_idx == 2 ? YELLOW : WHITE);
}

void Game::drawOnlineModeSelection() const {
  const std::string instructions_text =
      "Prefer CooP or PvP? Choose an option below";
  DrawText(instructions_text.c_str(),
           (shared::SCREEN_WIDTH - MeasureText(instructions_text.c_str(), 20)) /
               2,
           shared::SCREEN_HEIGHT / 2 - 40, 20, WHITE);

  const std::string confirm_text =
      "Use LEFT/RIGHT to choose and ENTER to confirm";
  const auto confirm_text_width = MeasureText(confirm_text.c_str(), 20);
  const auto confirm_text_offset_x =
      (shared::SCREEN_WIDTH - confirm_text_width) / 2;
  DrawText(confirm_text.c_str(), confirm_text_offset_x,
           shared::SCREEN_HEIGHT / 2 - 10, 20, WHITE);

  const std::string coopText = "CooP";
  const std::string _1v1Text = "1 Vs 1";
  const std::string _2v2Text = "2 Vs 2";
  DrawText(coopText.c_str(), confirm_text_offset_x,
           shared::SCREEN_HEIGHT / 2 + 40, 24,
           online_config_.mode_idx == 0 ? YELLOW : WHITE);

  DrawText(_1v1Text.c_str(),
           confirm_text_offset_x + confirm_text_width / 2 -
               MeasureText(_1v1Text.c_str(), 24) / 2,
           shared::SCREEN_HEIGHT / 2 + 40, 24,
           online_config_.mode_idx == 1 ? YELLOW : WHITE);

  DrawText(_2v2Text.c_str(),
           confirm_text_offset_x + confirm_text_width -
               MeasureText(_2v2Text.c_str(), 24),
           shared::SCREEN_HEIGHT / 2 + 40, 24,
           online_config_.mode_idx == 2 ? YELLOW : WHITE);
}

void Game::drawInputTextBox() const {
  const char *text =
      "Choose a name for the online game. Press Enter to continue";
  DrawText(text, (shared::SCREEN_WIDTH - MeasureText(text, 20)) / 2,
           name_text_box.y - 50.0f, 20, WHITE);

  DrawRectangleRec(name_text_box, Fade(LIGHTGRAY, 0.5f));
  DrawRectangleLines((int)name_text_box.x, (int)name_text_box.y,
                     (int)name_text_box.width, (int)name_text_box.height, RED);

  DrawText(online_config_.player_name, (int)name_text_box.x + 5,
           (int)name_text_box.y + 8, 40, WHITE);

  const auto nameLengthText = TextFormat(
      "INPUT CHARS: %i/%i", name_letters_count_, shared::MAX_NAME_LENGTH);
  DrawText(nameLengthText,
           (shared::SCREEN_WIDTH - MeasureText(nameLengthText, 20)) / 2.0f,
           name_text_box.y + name_text_box.height + 50.0f, 20, WHITE);

  if (showPlayerNameError_) {
    const char *errorText = "Please enter a name before continuing!";
    DrawText(errorText,
             (shared::SCREEN_WIDTH - MeasureText(errorText, 20)) / 2.0f,
             name_text_box.y + name_text_box.height + 80.0f, 20, RED);
  }
}

void Game::drawLobby() const {
  if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
    auto &lobbyUpdate = s->getLobbyUpdate();

    const char *title = "Lobby";
    float titleY = shared::SCREEN_HEIGHT / 2.0f - 100;
    DrawText(title, getCenteredTextX(title, 32), titleY, 32, WHITE);

    auto y = titleY + 60;

    std::visit(
        shared::overloaded{
            [&y, &s](const shared::CoopGameLobbyUpdate &u) {
              int readyCount = 0;

              for (std::size_t idx = 0; idx < u.players.size(); ++idx) {
                const auto &p = u.players[idx];

                if (!p.has_value()) {
                  const char *waitingText =
                      TextFormat("Waiting for a player...", idx + 1);
                  DrawText(waitingText, getCenteredTextX(waitingText, 20), y,
                           20, DARKGRAY);
                  y += 32;
                  continue;
                }

                if (p->is_ready)
                  ++readyCount;

                const char *status = p->is_ready ? "READY" : "Waiting...";
                auto statusColor = p->is_ready ? GREEN : LIGHTGRAY;

                std::string label =
                    p->name + (p->id == s->getPlayerId() ? " (You)" : "");
                auto labelWidth = MeasureText(label.c_str(), 20);
                auto labelPosX = (shared::SCREEN_WIDTH - labelWidth -
                                  MeasureText(status, 20) - 150) /
                                 2.0f;

                DrawText(label.c_str(), labelPosX, y, 20, WHITE);
                DrawText(status, labelPosX + labelWidth + 150, y, 20,
                         statusColor);
                y += 32;
              }

              y += 12;
              const char *progess =
                  TextFormat("Players ready: %d/%d", readyCount, u.max_players);
              DrawText(progess, (int)getCenteredTextX(progess, 20), y, 20,
                       LIGHTGRAY);
              y += 32;
            },
            [&y, &s](const shared::PvPGameLobbyUpdate &u) {
              const char *subtitle =
                  TextFormat("%dv%d", u.team_size, u.team_size);

              DrawText(subtitle, (int)getCenteredTextX(subtitle, 18), (int)y,
                       18, DARKGRAY);
              y += 36;

              float leftX = shared::SCREEN_WIDTH / 2.0f - 260.0f;
              float rightX = shared::SCREEN_WIDTH / 2.0f + 40.0f;
              float headerY = y;
              int totalReady = 0;
              float colBottom = y;

              for (std::size_t team_idx = 0; team_idx < u.teams.size();
                   ++team_idx) {
                const auto &team = u.teams[team_idx];
                Color teamColor = team_idx == 0 ? SKYBLUE : PINK;
                float colX = team_idx == 0 ? leftX : rightX;
                float rowY = headerY;

                const char *header = team_idx == 0 ? "Team 1" : "Team 2";
                DrawText(header, (int)colX, (int)rowY, 22, teamColor);
                rowY += 34;

                for (std::size_t p_idx = 0;
                     p_idx < u.team_size && p_idx < team.size(); ++p_idx) {
                  const auto &p = team[p_idx];

                  if (!p.has_value()) {
                    DrawText("Waiting for a player...", (int)colX, (int)rowY,
                             18, DARKGRAY);
                    rowY += 28;
                    continue;
                  }

                  if (p->is_ready)
                    ++totalReady;

                  const char *status = p->is_ready ? "READY" : "Waiting...";
                  auto statusColor = p->is_ready ? GREEN : LIGHTGRAY;

                  std::string label =
                      p->name + (p->id == s->getPlayerId() ? " (You)" : "");

                  DrawText(label.c_str(), (int)colX, (int)rowY, 18, WHITE);
                  DrawText(status, (int)(colX + 200), (int)rowY, 18,
                           statusColor);
                  rowY += 28;
                }

                colBottom = std::max(colBottom, rowY);
              }

              DrawLine(shared::SCREEN_WIDTH / 2, (int)headerY,
                       shared::SCREEN_WIDTH / 2, (int)colBottom,
                       Fade(WHITE, 0.15f));

              y = colBottom + 12;
              const char *progress = TextFormat("Players ready: %d/%d",
                                                totalReady, u.team_size * 2);
              DrawText(progress, (int)getCenteredTextX(progress, 20), (int)y,
                       20, LIGHTGRAY);
              y += 32;
            }},
        lobbyUpdate);

    const char *prompt = online_config_.is_ready ? "Press SPACE to cancel ready"
                                                 : "Press SPACE tp ready up";
    DrawText(prompt, (int)getCenteredTextX(prompt, 20), y, 20, LIGHTGRAY);

    const char *leave_prompt = "Press ESC to leave the lobby";
    DrawText(leave_prompt, (int)getCenteredTextX(leave_prompt, 20), y + 32, 20,
             LIGHTGRAY);
  }
}

void Game::drawGame() const {
  std::visit(
      shared::overloaded{
          [&](const shared::CoopGameState &s) {
            player_.draw(session_.get(), s.players);

            for (const auto &b : s.bullets)
              drawBullet(b);

            drawParticles();

            for (std::size_t idx = 0; idx < s.enemies.size(); ++idx) {
              if (s.enemies[idx][0] == 1) {
                float pos_x = s.enemies_offset_x +
                              (idx % shared::EnemiesPoolSimState::COLS) *
                                  (shared::EnemySimState::WIDTH +
                                   shared::EnemiesPoolSimState::SPACING_X);
                float pos_y = s.enemies_offset_y -
                              (idx / shared::EnemiesPoolSimState::COLS) *
                                  (shared::EnemySimState::HEIGHT +
                                   shared::EnemiesPoolSimState::SPACING_Y);
                Enemy::draw(pos_x, shared::SCREEN_HEIGHT - pos_y,
                            static_cast<shared::EnemyType>(s.enemies[idx][1]));
              }
            }

            {
              using GamePhase = shared::CoopGameSim::Phase;
              boss_.draw(s.boss, static_cast<GamePhase>(s.phase) ==
                                     GamePhase::FIGHT_BOSS);
            }
          },
          [&](const shared::PvPGameState &s) {
            player_.draw(session_.get(), s.teams, s.team_size);

            for (const auto &b : s.bullets)
              drawBullet(b);
          }},
      state_);

  if (screen_ == Screen::PAUSED) {
    DrawRectangle(0, 0, shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT,
                  Fade(BLACK, 0.6f));
    DrawText("Game Paused", shared::SCREEN_WIDTH / 2 - 100,
             shared::SCREEN_HEIGHT / 2, 20, WHITE);
    DrawText("Press ESC to resume", shared::SCREEN_WIDTH / 2 - 100,
             shared::SCREEN_HEIGHT / 2 + 30, 20, WHITE);
  }
}

void Game::drawEndScreen() const {
  // Draw a centered panel with final scores and animated border
  bool isPvP = std::holds_alternative<shared::PvPGameState>(state_);
  const int boxW = isPvP ? 640 : 500;
  const int boxH = isPvP ? 280 : 220;
  const float bx = shared::SCREEN_WIDTH / 2.0f - boxW / 2.0f;
  const float by = shared::SCREEN_HEIGHT / 2.0f - boxH / 2.0f;
  Rectangle rec{bx, by, (float)boxW, (float)boxH};
  DrawRectangleRec(rec, Fade(BLACK, 0.75f));

  // animated border pulse
  float pulse = (sinf(score_anim_time_ * 3.0f) * 0.5f + 0.5f);
  Color borderCol = Fade(YELLOW, 0.4f + 0.6f * pulse);
  DrawRectangleLinesEx(rec, 4, borderCol);

  std::string title{};
  std::visit(shared::overloaded{[&](const shared::CoopGameState &s) {
                                  using GamePhase = shared::CoopGameSim::Phase;
                                  auto phase = static_cast<GamePhase>(s.phase);
                                  if (phase == GamePhase::GAME_OVER)
                                    title = "Game Over";
                                  else if (phase == GamePhase::WON)
                                    title = "Victory!";
                                },
                                [&](const shared::PvPGameState &s) {
                                  using GamePhase = shared::PvPGameSim::Phase;
                                  auto phase = static_cast<GamePhase>(s.phase);

                                  if (phase == GamePhase::END) {
                                    title = "RESULTS";
                                  }
                                }},
             state_);

  DrawText(title.c_str(),
           (int)(shared::SCREEN_WIDTH / 2 - MeasureText(title.c_str(), 32) / 2),
           (int)(by + 12), 32, WHITE);

  bool isOnlineSession = false;
  uint32_t playerId = 1;
  if (OnlineSession *session = dynamic_cast<OnlineSession *>(session_.get())) {
    isOnlineSession = true;
    playerId = session->getPlayerId();
  }

  std::visit(shared::overloaded{
                 [&](const shared::CoopGameState &s) {
                   int y = (int)(by + 60);
                   auto maxScoreIt =
                       std::max_element(s.players.begin(), s.players.end(),
                                        [](const auto &a, const auto &b) {
                                          if (!a.has_value())
                                            return true;
                                          if (!b.has_value())
                                            return false;
                                          return a->points < b->points;
                                        });

                   for (std::size_t idx = 0; idx < s.players.size(); ++idx) {
                     const auto &p = s.players[idx];
                     if (!p.has_value())
                       continue;

                     auto hasHighestScore = (maxScoreIt != s.players.end() &&
                                             maxScoreIt->has_value() &&
                                             maxScoreIt->value().id == p->id);

                     std::string playerLabel;

                     if (p->name.empty())
                       playerLabel = "Player " + std::to_string(idx + 1);
                     else
                       playerLabel = p->name;

                     if (isOnlineSession) {
                       if (playerId == p->id) {
                         playerLabel =
                             playerLabel + " (You" +
                             (hasHighestScore ? " | Best Player" : "") + ")";
                       } else if (hasHighestScore) {
                         playerLabel += " (Best Player)";
                       }
                     } else if (hasHighestScore) {
                       playerLabel += " (Best Player)";
                     }

                     const auto line =
                         playerLabel +
                         TextFormat(": %u pts | %u lives", p->points, p->lives);

                     Color col = (hasHighestScore) ? YELLOW : LIGHTGRAY;
                     DrawText(line.c_str(), (int)(bx + 24), y, 22, col);
                     y += 32;
                   }
                 },
                 [&](const shared::PvPGameState &state) {
                   const shared::PlayerState *playerWithTheMaxScore = nullptr;

                   for (const auto &team : state.teams) {
                     for (const auto &player : team.players) {
                       if (!player.has_value())
                         continue;

                       if (!playerWithTheMaxScore ||
                           player->points > playerWithTheMaxScore->points) {
                         playerWithTheMaxScore = &player.value();
                       }
                     }
                   }

                   float leftX = bx + 28.0f;
                   float rightX = bx + boxW / 2.0f + 20.0f;
                   int headerY = (int)(by + 60);

                   for (std::size_t team_idx = 0; team_idx < state.teams.size();
                        ++team_idx) {
                     const auto &team = state.teams[team_idx];
                     Color teamColor = team_idx == 0 ? SKYBLUE : PINK;
                     float colX = team_idx == 0 ? leftX : rightX;
                     int y = headerY;

                     using TeamOutcome = shared::PvPGameSim::TeamOutcome;
                     auto outcome = static_cast<TeamOutcome>(team.outcome);
                     const char *outcomeText = "";
                     Color outcomeColor = LIGHTGRAY;
                     switch (outcome) {
                     case TeamOutcome::WON:
                       outcomeText = "WON";
                       outcomeColor = GOLD;
                       break;
                     case TeamOutcome::LOST:
                       outcomeText = "LOST";
                       outcomeColor = DARKGRAY;
                       break;
                     case TeamOutcome::DREW:
                       outcomeText = "DREW";
                       outcomeColor = SKYBLUE;
                       break;
                     case TeamOutcome::PLAYING:
                       break;
                     }

                     if (outcomeText[0]) {
                       DrawText(outcomeText, (int)colX, y, 14, outcomeColor);
                       y += 20;
                     }

                     const char *header = team_idx == 0 ? "Team 1" : "Team 2";
                     DrawText(header, (int)colX, y, 18, teamColor);
                     y += 30;

                     for (std::size_t p_idx = 0; p_idx < team.players.size();
                          ++p_idx) {
                       const auto &player = team.players[p_idx];
                       if (!player.has_value())
                         continue;

                       bool isMvp = playerWithTheMaxScore == &player.value();

                       std::string label;
                       if (player->name.empty())
                         label = "Player " + std::to_string(p_idx + 1);
                       else
                         label = player->name;

                       if (isOnlineSession && playerId == player->id)
                         label += " (You)";
                       if (isMvp)
                         label += " (MVP)";

                       const auto line =
                           label + TextFormat(": %u pts | %u lives",
                                              player->points, player->lives);

                       Color col = isMvp ? YELLOW : WHITE;
                       DrawText(line.c_str(), (int)colX, y, 16, col);
                       y += 24;
                     }
                   }
                 }},
             state_);

  const char *controls = "Press R to restart or ENTER to return to menu";
  DrawText(controls,
           (int)(shared::SCREEN_WIDTH / 2 - MeasureText(controls, 18) / 2),
           (int)(by + boxH - 30), 18, LIGHTGRAY);
}
