#include "game.h"
#include "constants.h"
#include "entities/enemy.h"
#include "entities/player.h"
#include "entities/star.h"
#include "raylib.h"
#include "session.h"
#include "shared/messages.h"
#include "shared/sim/enemy_sim.h"
#include "shared/sim/game_sim.h"
#include <cfloat>
#include <cmath>
#include <cstdlib>

void Game::init() {
  screen_ = Screen::MENU;

  state_ = shared::GameState();

  for (auto &s : stars_)
    s.initRandom(shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT);
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
  InitWindow(shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT, "SUPER GAME");
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

    while (accumulator >= shared::FIXED_DT) {
      for (auto &s : stars_)
        s.update(shared::FIXED_DT, shared::SCREEN_HEIGHT, shared::SCREEN_WIDTH);

      std::array<shared::PlayerInput, shared::MAX_PLAYERS> inputs;
      inputs[0].buttons = getPlayerInputs();
      inputs[0].player_id = 1;

      if (screen_ == Screen::CONNECTING) {
        if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
          if (s->getPlayerId() != 0) {
            screen_ = Screen::LOBBY;
          }
        }
      }

      if (screen_ == Screen::LOBBY) {
        if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
          if (s->getLobbyUpdate().game_started) {
            screen_ = Screen::PLAYING;
          }
        }
      }

      if (screen_ == Screen::PLAYING) {
        auto previous_state = state_;
        if (session_)
          state_ = session_->step(inputs[0], shared::FIXED_DT);
        spawnEnemyExplosions(previous_state, state_);
      }

      if (screen_ != Screen::GAME_OVER && screen_ != Screen::WIN) {
        auto phase = static_cast<shared::GamePhase>(state_.phase);

        if (phase == shared::GamePhase::GAME_OVER ||
            phase == shared::GamePhase::WON) {
          session_ = nullptr;
          if (phase == shared::GamePhase::GAME_OVER)
            screen_ = Screen::GAME_OVER;
          if (phase == shared::GamePhase::WON)
            screen_ = Screen::WIN;
        }
      }

      accumulator -= shared::FIXED_DT;
    }

    // Update particles using the real frame time so they feel smooth
    updateParticles(frame_time);

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
    if (IsKeyPressed(KEY_LEFT)) {
      mode_ = GameMode::LOCAL;
    } else if (IsKeyPressed(KEY_RIGHT)) {
      mode_ = GameMode::ONLINE;
    } else if (IsKeyPressed(KEY_ENTER)) {
      if (mode_ == GameMode::ONLINE) {
        session_ = std::make_unique<OnlineSession>("ws://localhost:9001");
        screen_ = Screen::CONNECTING;
      } else {
        session_ = std::make_unique<LocalSession>();
        screen_ = Screen::PLAYING;
      }
    }
    break;

  case Screen::CONNECTING:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::MENU;
    }
    break;

  case Screen::LOBBY:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::MENU;
    }

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
  case Screen::MENU:
    DrawText("Use LEFT/RIGHT to choose mode", shared::SCREEN_WIDTH / 2 - 180,
             shared::SCREEN_HEIGHT / 2 - 40, 20, WHITE);
    DrawText("Press ENTER to confirm", shared::SCREEN_WIDTH / 2 - 150,
             shared::SCREEN_HEIGHT / 2 - 10, 20, WHITE);
    DrawText("Local", shared::SCREEN_WIDTH / 2 - 150,
             shared::SCREEN_HEIGHT / 2 + 40, 24,
             mode_ == GameMode::LOCAL ? YELLOW : WHITE);
    DrawText("Online", shared::SCREEN_WIDTH / 2 + 50,
             shared::SCREEN_HEIGHT / 2 + 40, 24,
             mode_ == GameMode::ONLINE ? YELLOW : WHITE);
    break;

  case Screen::CONNECTING:
    DrawText("Connecting to server...", shared::SCREEN_WIDTH / 2 - 170,
             shared::SCREEN_HEIGHT / 2, 24, LIGHTGRAY);
    DrawText("Press ESC to cancel", shared::SCREEN_WIDTH / 2 - 140,
             shared::SCREEN_HEIGHT / 2 + 40, 20, WHITE);
    break;

  case Screen::LOBBY: {
    if (OnlineSession *s = dynamic_cast<OnlineSession *>(session_.get())) {
      auto &lobbyUpdate = s->getLobbyUpdate();
      const auto text = "Waiting for players to join (" +
                        std::to_string(lobbyUpdate.player_count) + "/" +
                        std::to_string(lobbyUpdate.max_players) + " players)";
      DrawText(text.c_str(),
               (shared::SCREEN_WIDTH - MeasureText(text.c_str(), 20)) / 2,
               shared::SCREEN_HEIGHT / 2, 20, WHITE);
    }
    break;
  }

  case Screen::PLAYING:
  case Screen::PAUSED: {
    player_.draw(session_.get(), state_.players);

    for (const auto &b : state_.bullets)
      drawBullet(b);

    // draw particle explosions (spawned when enemies die)
    drawParticles();

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
      DrawRectangle(0, 0, shared::SCREEN_WIDTH, shared::SCREEN_HEIGHT,
                    Fade(BLACK, 0.6f));
      DrawText("Game Paused", shared::SCREEN_WIDTH / 2 - 100,
               shared::SCREEN_HEIGHT / 2, 20, WHITE);
      DrawText("Press ESC to resume", shared::SCREEN_WIDTH / 2 - 100,
               shared::SCREEN_HEIGHT / 2 + 30, 20, WHITE);
    }
    break;
  }

  case Screen::GAME_OVER:
  case Screen::WIN:
    DrawText(screen_ == Screen::GAME_OVER ? "Game Over" : "You Win!",
             shared::SCREEN_WIDTH / 2 - 100, shared::SCREEN_HEIGHT / 2, 20,
             WHITE);
    DrawText("Press ENTER to return to menu", shared::SCREEN_WIDTH / 2 - 150,
             shared::SCREEN_HEIGHT / 2 + 30, 20, WHITE);

    break;
  }

  DrawFPS(10, 10);
}

// --- Particle system implementation ---

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

void Game::spawnEnemyExplosions(const shared::GameState &before,
                                const shared::GameState &after) {
  for (std::size_t idx = 0; idx < after.enemies.size(); ++idx) {
    bool was_alive = before.enemies[idx][0] != 0;
    bool is_alive = after.enemies[idx][0] != 0;

    if (was_alive && !is_alive) {
      float pos_x =
          after.enemies_offset_x + (idx % shared::EnemiesPoolSimState::COLS) *
                                       (shared::EnemySimState::WIDTH +
                                        shared::EnemiesPoolSimState::SPACING_X);
      float pos_y =
          after.enemies_offset_y + (idx / shared::EnemiesPoolSimState::COLS) *
                                       (shared::EnemySimState::HEIGHT +
                                        shared::EnemiesPoolSimState::SPACING_Y);

      spawnExplosion({pos_x, pos_y},
                     static_cast<shared::EnemyType>(after.enemies[idx][1]));
    }
  }
}
