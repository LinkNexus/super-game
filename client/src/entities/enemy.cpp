#include "entities/enemy.h"

Texture2D Enemy::texture_type1_ = {};
Texture2D Enemy::texture_type2_ = {};

void Enemy::loadTextures() {
  texture_type1_ = LoadTexture("assets/enemy1.png");
  texture_type2_ = LoadTexture("assets/enemy2.png");
}

void Enemy::unloadTextures() {
  UnloadTexture(texture_type1_);
  UnloadTexture(texture_type2_);
}

void Enemy::draw(float pos_x, float pos_y, shared::EnemyType type) {
  const Texture2D &tex =
      (type == shared::EnemyType::TYPE_1) ? texture_type1_ : texture_type2_;

  if (tex.id != 0) {
    float scale = shared::EnemySimState::WIDTH / tex.width;
    Vector2 draw_pos = {pos_x - (tex.width * scale) / 2.0f,
                        pos_y - (tex.height * scale) / 2.0f};
    DrawTextureEx(tex, draw_pos, 0.0f, scale, WHITE);
  } else {
    Color c = (type == shared::EnemyType::TYPE_1) ? GREEN : ORANGE;
    DrawRectangle((int)pos_x - shared::EnemySimState::WIDTH / 2,
                  (int)pos_y - shared::EnemySimState::HEIGHT / 2,
                  shared::EnemySimState::WIDTH, shared::EnemySimState::HEIGHT,
                  c);
  }
}
