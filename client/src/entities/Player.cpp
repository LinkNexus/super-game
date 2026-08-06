#include "entities/player.h"
#include "constants.h"
#include "raylib.h"
#include "shared/math_utils.h"
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

void Player::drawPlayer(const shared::Vec2D position, bool main) const {
  const auto size = shared::PlayerSimState::SIZE;

  if (texture.id != 0) {
    float scale = (size * 2.0f) / texture.width;
    Vector2 draw_pos = {position.x - (texture.width * scale) / 2.0f,
                        position.y - (texture.height * scale) / 2.0f};

    Color tint = !main ? SKYBLUE : WHITE;
    DrawTextureEx(texture, draw_pos, 0.0f, scale, tint);
  } else {
    Vector2 tip = {position.x, position.y - size};
    Vector2 left = {position.x - size * 0.7f, position.y + size * 0.7f};
    Vector2 right = {position.x + size * 0.7f, position.y + size * 0.7f};
    Color shipColor = main ? PURPLE : SKYBLUE;
    DrawTriangle(tip, left, right, shipColor);
    DrawTriangleLines(tip, left, right, WHITE);
  }
}

void Player::drawLives(uint8_t lives, float y_offset) const {
  for (int i = 0; i < lives; i++) {
    float x = shared::SCREEN_WIDTH - 10 - HEART_SIZE -
              i * (HEART_SIZE + HEART_SPACING);

    if (heart_texture_.id != 0) {
      float scale = HEART_SIZE / heart_texture_.width;
      DrawTextureEx(heart_texture_, {x, y_offset}, 0.0f, scale, WHITE);
    } else {
      float cx = x + HEART_SIZE / 2.0f;
      float lobe_r = HEART_SIZE * 0.28f;
      float lobe_y = y_offset + lobe_r;
      DrawCircle(cx - lobe_r, lobe_y, lobe_r, RED);
      DrawCircle(cx + lobe_r, lobe_y, lobe_r, RED);
      Vector2 bottom = {cx, y_offset + HEART_SIZE};
      Vector2 leftPt = {x, lobe_y};
      Vector2 rightPt = {x + HEART_SIZE, lobe_y};
      DrawTriangle(leftPt, bottom, rightPt, RED);
    }
  }
}
