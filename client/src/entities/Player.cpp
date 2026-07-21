#include "entities/player.h"
#include "constants.h"
#include "raylib.h"
#include "shared/player_sim.h"

void Player::loadTexture() { texture = LoadTexture("assets/playerShip.png"); }

void Player::unload() { UnloadTexture(texture); }

void Player::draw(const shared::PlayerSimState &state) const {
  auto size = shared::PlayerSimState::SIZE;

  if (texture.id != 0) {
    float scale = (size * 2.0f) / texture.width;
    Vector2 draw_pos = {state.position.x - (texture.width * scale) / 2.0f,
                        state.position.y - (texture.height * scale) / 2.0f};
    DrawTextureEx(texture, draw_pos, 0.0f, scale, WHITE);
  } else {
    Vector2 tip = {state.position.x, state.position.y - size};
    Vector2 left = {state.position.x - size * 0.7f,
                    state.position.y + size * 0.7f};
    Vector2 right = {state.position.x + size * 0.7f,
                     state.position.y + size * 0.7f};
    DrawTriangle(tip, left, right, SKYBLUE);
    DrawTriangleLines(tip, left, right, WHITE);
  }

  const char *lives_text = TextFormat("Lives: %d", state.lives);
  DrawText(lives_text,
           SCREEN_WIDTH - 10 - MeasureText(lives_text, STATUS_FONT_SIZE), 10,
           STATUS_FONT_SIZE, WHITE);

  const char *points_text = TextFormat("Points: %d", state.points);
  DrawText(points_text,
           SCREEN_WIDTH - 10 - MeasureText(points_text, STATUS_FONT_SIZE),
           10 + STATUS_FONT_SIZE, STATUS_FONT_SIZE, WHITE);
}
