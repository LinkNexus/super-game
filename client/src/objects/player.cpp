#include "objects/player.h"
#include "constants.h"

void Player::update(float dt) {
  float vx = 0;
  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))   vx = -1.0f;
  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))  vx =  1.0f;

  position.x += vx * speed * dt;

  if (position.x < size)                  position.x = size;
  if (position.x > SCREEN_WIDTH - size)   position.x = SCREEN_WIDTH - size;
  if (position.y < size)                  position.y = size;
  if (position.y > SCREEN_HEIGHT - size)  position.y = SCREEN_HEIGHT - size;
}

void Player::draw() const {
  Vector2 tip   = {position.x,               position.y - size};
  Vector2 left  = {position.x - size * 0.7f, position.y + size * 0.7f};
  Vector2 right = {position.x + size * 0.7f, position.y + size * 0.7f};

  DrawTriangle(tip, left, right, SKYBLUE);
  DrawTriangleLines(tip, left, right, WHITE);
}
