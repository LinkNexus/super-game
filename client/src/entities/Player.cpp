#include "entities/player.h"
#include "constants.h"
#include "raylib.h"
#include "shared/messages.h"
#include "shared/sim/player_sim.h"

Texture2D Player::heart_texture_ = {};

void Player::loadTexture() {
  texture = LoadTexture("assets/playerShip.png");

  heart_texture_ = LoadTexture("assets/heart.png");
}

void Player::unload() {
  UnloadTexture(texture);
  if (heart_texture_.id != 0) {
    UnloadTexture(heart_texture_);
  }
}

void Player::draw(const shared::PlayerState &state) const {
  auto size = shared::PlayerSimState::SIZE;

  if (texture.id != 0) {
    float scale = (size * 2.0f) / texture.width;
    Vector2 draw_pos = {state.position.x - (texture.width * scale) / 2.0f,
                        state.position.y - (texture.height * scale) / 2.0f};
    // Tint player texture differently for player 2 (id == 1)
    Color tint = (state.id == 1) ? SKYBLUE : WHITE;
    DrawTextureEx(texture, draw_pos, 0.0f, scale, tint);
  } else {
    Vector2 tip = {state.position.x, state.position.y - size};
    Vector2 left = {state.position.x - size * 0.7f,
                    state.position.y + size * 0.7f};
    Vector2 right = {state.position.x + size * 0.7f,
                     state.position.y + size * 0.7f};
    Color shipColor = (state.id == 1) ? PURPLE : SKYBLUE;
    DrawTriangle(tip, left, right, shipColor);
    DrawTriangleLines(tip, left, right, WHITE);
  }

  for (int i = 0; i < state.lives; i++) {
    float x = SCREEN_WIDTH - 10 - HEART_SIZE - i * (HEART_SIZE + HEART_SPACING);
    float y = 10.0f;

    if (heart_texture_.id != 0) {
      float scale = HEART_SIZE / heart_texture_.width;
      DrawTextureEx(heart_texture_, {x, y}, 0.0f, scale, WHITE);
    } else {
      float cx = x + HEART_SIZE / 2.0f;
      float lobe_r = HEART_SIZE * 0.28f;
      float lobe_y = y + lobe_r;
      DrawCircle(cx - lobe_r, lobe_y, lobe_r, RED);
      DrawCircle(cx + lobe_r, lobe_y, lobe_r, RED);
      Vector2 bottom = {cx, y + HEART_SIZE};
      Vector2 leftPt = {x, lobe_y};
      Vector2 rightPt = {x + HEART_SIZE, lobe_y};
      DrawTriangle(leftPt, bottom, rightPt, RED);
    }
  }

  const char *points_text = TextFormat("Points: %d", state.points);
  DrawText(points_text,
           SCREEN_WIDTH - 10 - MeasureText(points_text, STATUS_FONT_SIZE),
           10 + HEART_SIZE, STATUS_FONT_SIZE, WHITE);
}
