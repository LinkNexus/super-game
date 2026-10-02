#include "game.h"
#include "constants.h"
#include "raylib.h"
#include "session.h"
#include "shared/aliases.h"
#include "shared/constants.h"
#include "shared/helpers.h"
#include "shared/messages.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/game_sim.h"
#include "types.h"
#include "utils.h"
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <string>

Game::Game(std::string_view server_url) : server_url_(std::move(server_url)) {}

void Game::restart() {
  if (type_ == GameType::LOCAL) {
    startLocalSession();
  } else {
    startOnlineSession();
  }
}

void Game::init() {
  screen_ = GameScreen::MENU;

  for (auto &s : stars_)
    s.initRandom();

  audio_manager_.initAudio();
  game_designer_.init();
}

void Game::pollPlayersInputs() {
  auto playersIds = session_->getPlayersIds();

  for (std::size_t i = 0; i < config_->player_count; ++i) {
    auto &input = inputs_[i];
    auto playerId = playersIds[i];

    input.buttons = shared::BUTTON_NONE;
    const auto &controls = PLAYERS_CONTROLS[i];

    for (auto control : controls) {
      if (IsKeyDown(control.first)) {
        input.player_id = playerId;
        input.buttons =
            static_cast<shared::Button>(input.buttons | control.second);
      }
    }
  }
}

void Game::checkAndPlaySoundsOnEvents() {
  // play sounds for events (enemy death, boss hit)
  if (audio_manager_.isAudioReady() && config_) {
    std::visit(
        shared::overloaded{
            [&](shared::CoopGameState &s, shared::CoopGameState &prev_s) {
              for (std::size_t idx = 0; idx < s.enemies.size(); ++idx) {
                if (prev_s.enemies[idx][0] == 1 && s.enemies[idx][0] == 0) {
                  audio_manager_.playExplosionSound();
                }
              }

              if (prev_s.boss.active && s.boss.active &&
                  prev_s.boss.health > s.boss.health) {
                audio_manager_.playBossHitSound();
              }

              spawnEnemyExplosions(prev_s, s);
            },
            [](shared::PvPGameState &, shared::PvPGameState &) {

            },
            [](auto &, auto &) {}},
        config_->state, config_->prev_state);
  }
}

void Game::handleEventsOnScreen() {
  if (screen_ == GameScreen::CONNECTING) {
    if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
      auto playerIds = s->getPlayersIds();

      if (std::any_of(playerIds.begin(), playerIds.end(),
                      [](const auto &id) { return id != 0; })) {
        screen_ = GameScreen::LOBBY;
      }
    }
  }

  if (screen_ == GameScreen::LOBBY) {
    if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
      std::visit(shared::overloaded{[this](const auto &u) {
                   if (u.game_started)
                     screen_ = GameScreen::PLAYING;
                 }},
                 s->getLobbyUpdate());
    }
  }

  if (screen_ == GameScreen::PLAYING) {
    pollPlayersInputs();

    if (session_)
      config_->state = session_->step(inputs_, shared::FIXED_DT);

    checkAndPlaySoundsOnEvents();

    config_->prev_state = config_->state;
  }

  if (screen_ != GameScreen::END && config_) {
    std::visit(
        shared::overloaded{[&](shared::CoopGameState &s) {
                             using GamePhase = shared::CoopGameSim::Phase;
                             auto phase = static_cast<GamePhase>(s.phase);

                             if (phase == GamePhase::GAME_OVER ||
                                 phase == GamePhase::WON) {
                               session_ = nullptr;
                               screen_ = GameScreen::END;
                             }
                           },
                           [&](shared::PvPGameState &s) {
                             using GamePhase = shared::PvPGameSim::Phase;
                             auto phase = static_cast<GamePhase>(s.phase);

                             if (phase == GamePhase::END) {
                               session_ = nullptr;
                               screen_ = GameScreen::END;
                             }
                           }},
        config_->state);
  } else {
    // animate score screen border while showing final results
    score_anim_time_ += shared::FIXED_DT;
  }
}

void Game::run() {
  InitWindow(shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT, "SUPER GAME");
  ChangeDirectory(GetApplicationDirectory());
  SetExitKey(KEY_NULL);
  SetTargetFPS(TARGET_FPS);

  init();

  float accumulator = 0.0f;

  while (!WindowShouldClose()) {
    handleInput();

    float frame_time = GetFrameTime();
    if (frame_time > 0.1f)
      frame_time = 0.1f;
    accumulator += frame_time;

    while (accumulator >= shared::FIXED_DT) {
      for (auto &s : stars_)
        s.update(shared::FIXED_DT);

      audio_manager_.updateMusicVolume(screen_ == GameScreen::PLAYING);

      handleEventsOnScreen();

      accumulator -= shared::FIXED_DT;
    }

    for (auto &p : particles_)
      p.update(frame_time);

    audio_manager_.updateBackgroundMusic();

    BeginDrawing();
    ClearBackground(BLACK);
    draw();
    EndDrawing();
  }

  game_designer_.unload();
  audio_manager_.unloadAudio();

  CloseWindow();
}

void Game::startLocalSession() {
  session_ = std::make_unique<LocalSession>(&config_->modes[config_->mode_idx],
                                            config_->player_count);
  screen_ = GameScreen::PLAYING;

  config_->state = shared::GameState();
  config_->prev_state = config_->state;
}

void Game::startOnlineSession() {
  auto url = server_url_;
  auto config = dynamic_cast<OnlineConfig *>(config_.get());
  auto &mode = config->modes[config->mode_idx];

  for (std::size_t i = 0; i < config->player_count; ++i) {
    auto &name = config->players_names[i];

    url += std::string(i == 0 ? "?" : "&") + "name" + std::to_string(i + 1) +
           "=" + name;
  }

  url += "&slots=" + std::to_string(config->player_count);

  std::visit(shared::overloaded{
                 [&url](PvPMode &m) {
                   auto &matchUp = m.pvp_match_ups[m.selected_match_up_idx];

                   url += "&team_size=" + std::to_string(matchUp.second) +
                          "&team_count=" + std::to_string(matchUp.first);
                 },
                 [](auto &) {

                 }},
             mode);

  session_ = std::make_unique<OnlineSession>(url, config->player_count);
  screen_ = GameScreen::CONNECTING;
  config->state = shared::GameState();
  config->prev_state = config->state;
  config->are_ready = false;
}

void Game::initNameTextBoxes() {
  auto config = dynamic_cast<OnlineConfig *>(config_.get());

  for (std::size_t i = 0; i < config->player_count; ++i) {
    auto &box = name_text_boxes_[i];
    box.init(&config->players_names[i], shared::MAX_NAME_LENGTH);
  }
}

void Game::handleInput() {
  switch (screen_) {
  case GameScreen::MENU:
    if (IsKeyPressed(KEY_LEFT)) {
      type_ = GameType::LOCAL;
    } else if (IsKeyPressed(KEY_RIGHT)) {
      type_ = GameType::ONLINE;
    } else if (IsKeyPressed(KEY_ENTER)) {
      screen_ = GameScreen::SELECT_MODE;

      switch (type_) {
      case GameType::LOCAL:
        config_ = std::make_unique<LocalConfig>();
        break;
      case GameType::ONLINE:
        config_ = std::make_unique<OnlineConfig>();
        break;
      }
    }
    break;

  case GameScreen::SELECT_MODE:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = GameScreen::MENU;
      config_ = nullptr;
    } else if (IsKeyPressed(KEY_LEFT)) {
      if (config_->mode_idx > 0)
        config_->mode_idx--;
      else
        config_->mode_idx = config_->modes.size() - 1;
    } else if (IsKeyPressed(KEY_RIGHT)) {
      if (config_->mode_idx < config_->modes.size() - 1)
        config_->mode_idx++;
      else
        config_->mode_idx = 0;
    } else if (IsKeyPressed(KEY_ENTER)) {
      auto &mode = config_->modes[config_->mode_idx];

      std::visit(
          shared::overloaded{
              [this](CoopMode &) {
                screen_ = type_ == GameType::ONLINE
                              ? GameScreen::SELECT_PLAYERS_COUNT_ON_THIS_MACHINE
                              : GameScreen::SELECT_PLAYER_COUNT;
              },
              [this](PvPMode &m) {
                for (shared::PlayerCount teamCount = 2;
                     teamCount <= shared::MAX_TEAMS; ++teamCount) {
                  shared::PlayerCount maxPlayersCount =
                      type_ == GameType::ONLINE
                          ? shared::MAX_PLAYERS_PER_TEAM
                          : MAX_PLAYERS_ON_THIS_MACHINE / teamCount;

                  for (shared::PlayerCount playersCount = 1;
                       playersCount <= maxPlayersCount; ++playersCount) {
                    m.pvp_match_ups.push_back({teamCount, playersCount});
                  }
                }

                screen_ = GameScreen::SELECT_PLAYER_COUNT;
              }},
          mode);
    }
    break;

  case GameScreen::SELECT_PLAYER_COUNT: {
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = GameScreen::SELECT_MODE;
      break;
    }

    auto &mode = config_->modes[config_->mode_idx];

    std::visit(
        shared::overloaded{
            [](CoopMode &m) {
              if (IsKeyPressed(KEY_LEFT)) {
                m.players_count = (m.players_count > 1)
                                      ? m.players_count - 1
                                      : MAX_PLAYERS_ON_THIS_MACHINE;
              } else if (IsKeyPressed(KEY_RIGHT)) {
                m.players_count =
                    (m.players_count < MAX_PLAYERS_ON_THIS_MACHINE)
                        ? m.players_count + 1
                        : 1;
              }
            },
            [](PvPMode &m) {
              if (IsKeyPressed(KEY_LEFT)) {
                m.selected_match_up_idx = (m.selected_match_up_idx > 0)
                                              ? m.selected_match_up_idx - 1
                                              : m.pvp_match_ups.size() - 1;
              } else if (IsKeyPressed(KEY_RIGHT)) {
                m.selected_match_up_idx =
                    (m.selected_match_up_idx < m.pvp_match_ups.size() - 1)
                        ? m.selected_match_up_idx + 1
                        : 0;
              }
            }},
        mode);

    if (IsKeyPressed(KEY_ENTER)) {
      if (type_ == GameType::LOCAL) {
        std::visit(shared::overloaded{
                       [this](CoopMode &m) {
                         config_->player_count = m.players_count;
                       },
                       [this](PvPMode &m) {
                         auto &matchUp =
                             m.pvp_match_ups[m.selected_match_up_idx];
                         config_->player_count = matchUp.first * matchUp.second;
                       }},
                   mode);

        startLocalSession();
      } else {
        std::visit(
            shared::overloaded{
                [this](PvPMode &m) {
                  if (m.pvp_match_ups[m.selected_match_up_idx].second == 1) {
                    config_->player_count = 1;
                    initNameTextBoxes();
                    screen_ = GameScreen::NAME_ENTRY;
                  } else {
                    screen_ = GameScreen::SELECT_PLAYERS_COUNT_ON_THIS_MACHINE;
                  }
                },
                [this](auto &) {
                  screen_ = GameScreen::SELECT_PLAYERS_COUNT_ON_THIS_MACHINE;
                }},
            mode);
      }
    }

    break;
  }

  case GameScreen::SELECT_PLAYERS_COUNT_ON_THIS_MACHINE: {
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = GameScreen::SELECT_MODE;
    } else if (IsKeyPressed(KEY_LEFT)) {
      if (config_->player_count > 1)
        config_->player_count--;
      else
        config_->player_count = shared::MAX_PLAYERS_PER_CLIENT;
    } else if (IsKeyPressed(KEY_RIGHT)) {
      if (config_->player_count < shared::MAX_PLAYERS_PER_CLIENT)
        config_->player_count++;
      else
        config_->player_count = 1;
    } else if (IsKeyPressed(KEY_ENTER)) {
      initNameTextBoxes();
      screen_ = GameScreen::NAME_ENTRY;
    }

    break;
  }

  case GameScreen::NAME_ENTRY: {
    auto config = dynamic_cast<OnlineConfig *>(config_.get());
    int key = GetCharPressed();

    auto requestNextChar = [&key]() {
      key = GetCharPressed();
      return key;
    };

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      auto mousePos = GetMousePosition();
      for (std::size_t i = 0; i < config->player_count; ++i) {
        name_text_boxes_[i].setFocus(
            CheckCollisionPointRec(mousePos, name_text_boxes_[i].rect));
      }
    }

    for (std::size_t i = 0; i < config->player_count; ++i) {
      name_text_boxes_[i].update(key, requestNextChar);
    }

    if (IsKeyPressed(KEY_ENTER)) {
      if (std::all_of(name_text_boxes_.begin(),
                      name_text_boxes_.begin() + config->player_count,
                      [](auto &t) { return t.getError().empty(); })) {
        startOnlineSession();
      }
    }

    if (IsKeyPressed(KEY_ESCAPE)) {
      for (std::size_t i = 0; i < config->player_count; ++i) {
        config->players_names[i] = "";
        name_text_boxes_[i] = TextInput();
      }
      screen_ = GameScreen::SELECT_PLAYERS_COUNT_ON_THIS_MACHINE;
    }

    break;
  }

  case GameScreen::CONNECTING:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = GameScreen::NAME_ENTRY;
    }
    break;

  case GameScreen::LOBBY: {
    auto config = dynamic_cast<OnlineConfig *>(config_.get());
    auto session = dynamic_cast<OnlineSession *>(session_.get());

    if (IsKeyPressed(KEY_ESCAPE)) {
      session_ = nullptr;
      config_ = nullptr;
      screen_ = GameScreen::MENU;
    }

    if (IsKeyPressed(KEY_SPACE)) {
      config->are_ready = !config->are_ready;
      session->sendReady(config->are_ready);
    }

    break;
  }

  case GameScreen::PLAYING:
    if (IsKeyPressed(KEY_ESCAPE))
      screen_ = GameScreen::PAUSED;
    if (IsKeyPressed(KEY_SPACE)) {
      audio_manager_.playShootSound();
    }
    break;

  case GameScreen::PAUSED:
    if (IsKeyPressed(KEY_ESCAPE))
      screen_ = GameScreen::PLAYING;

    break;

  case GameScreen::END:
    if (IsKeyPressed(KEY_ENTER))
      init();
    if (IsKeyPressed(KEY_R))
      restart();

    break;
  }
}

void Game::draw() {
  game_designer_.drawStars(stars_);

  switch (screen_) {
  case GameScreen::MENU:
    game_designer_.drawMainMenu(type_);
    break;

  case GameScreen::SELECT_MODE:
    game_designer_.drawModeSelection(config_.get());
    break;

  case GameScreen::SELECT_PLAYER_COUNT: {
    auto &mode = config_->modes[config_->mode_idx];

    std::visit(shared::overloaded{
                   [this](CoopMode &m) {
                     game_designer_.drawPlayerCountSelectionCoop(
                         m.players_count);
                   },
                   [this](PvPMode &m) {
                     game_designer_.drawPvPSchemaSelection(
                         m.pvp_match_ups[m.selected_match_up_idx].first,
                         m.pvp_match_ups[m.selected_match_up_idx].second,
                         m.pvp_match_ups);
                   }},
               mode);

    break;
  }

  case GameScreen::SELECT_PLAYERS_COUNT_ON_THIS_MACHINE:
    game_designer_.drawOnlinePlayerCountSelection(config_->player_count);
    break;

  case GameScreen::NAME_ENTRY:
    game_designer_.drawTextInputs(name_text_boxes_, config_->player_count);
    break;

  case GameScreen::CONNECTING:
    game_designer_.drawConnectingScreen();
    break;

  case GameScreen::LOBBY:
    game_designer_.drawLobby(
        dynamic_cast<OnlineSession *>(session_.get()),
        dynamic_cast<OnlineConfig *>(config_.get())->are_ready);
    break;

  case GameScreen::PLAYING:
  case GameScreen::PAUSED:
    game_designer_.drawGame(config_->state, session_.get(), type_, screen_,
                            particles_);
    break;

  case GameScreen::END:
    game_designer_.drawEndScreen();
    break;
  }

  DrawFPS(10, 10);
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
