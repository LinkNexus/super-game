#include "Player.h"

void Player::load_texture() {
  texture = LoadTexture("assets/playerShip.png");
}

void Player::update(float dt) {
  float vec_x = 0;

  if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))
    vec_x = -1.0f;
  if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT))
    vec_x = 1.0f;

  position.x += vec_x * speed * dt;

  if (position.x < size)
    position.x = size;
  if (position.x > 1200 - size) // TODO: SCREEN_WIDTH einbinden statt Zahl
    position.x = 1200 - size;
}

void Player::draw() {
  // Skaliere die Textur so, dass sie ungefähr `size`-passend zum alten Dreieck ist
  float scale = (size * 2.0f) / texture.width;

  // DrawTextureEx nimmt die OBERE LINKE ECKE als position,
  // wir wollen aber, dass `position` die MITTE des Schiffs ist -> Offset abziehen
  Vector2 drawPos = {
    position.x - (texture.width * scale) / 2.0f,
    position.y - (texture.height * scale) / 2.0f
  };

  DrawTextureEx(texture, drawPos, rotation, scale, WHITE);
}

void Player::unload() {
  UnloadTexture(texture);
}