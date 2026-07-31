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
#include <cmath>
#include <algorithm>

void Game::init() {
  screen_ = Screen::MENU;

  std::array<uint8_t, MAX_PLAYERS> player_ids;
  player_ids.fill(0);

  sim_ = shared::GameSim();
  sim_.start(player_ids);

  state_ = shared::GameState();

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
        auto previous_state = state_;
        sim_.step(state_, inputs, FIXED_DT);
        spawnEnemyExplosions(previous_state, state_);
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
    if (IsKeyPressed(KEY_ENTER)) {
      screen_ = Screen::PLAYING;
    }
    break;

  case Screen::PLAYING:
    if (IsKeyPressed(KEY_ESCAPE)) {
      screen_ = Screen::PAUSED;
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
    break;

  case Screen::WIN:
    if (IsKeyPressed(KEY_ENTER)) {
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
    DrawText("Press ENTER to start the game", SCREEN_WIDTH / 2 - 150,
             SCREEN_HEIGHT / 2, 20, WHITE);
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
      DrawRectangleV({p.position.x - p.size / 2.0f, p.position.y - p.size / 2.0f},
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
      float pos_x = after.enemies_offset_x +
                    (idx % shared::EnemiesPoolSimState::COLS) *
                        (shared::EnemySimState::WIDTH +
                         shared::EnemiesPoolSimState::SPACING_X);
      float pos_y = after.enemies_offset_y +
                    (idx / shared::EnemiesPoolSimState::COLS) *
                        (shared::EnemySimState::HEIGHT +
                         shared::EnemiesPoolSimState::SPACING_Y);

      spawnExplosion({pos_x, pos_y},
                     static_cast<shared::EnemyType>(after.enemies[idx][1]));
    }
  }
}
