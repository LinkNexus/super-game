#include "objects/enemy.h"
#include "objects/bullet.h"

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

void Enemy::draw() const {
  if (!alive)
    return;

  const Texture2D &tex = (type == EnemyType::TYPE_1) ? texture_type1_ : texture_type2_;

  if (tex.id != 0) {
    float scale = WIDTH / tex.width;
    Vector2 draw_pos = {
      position.x - (tex.width * scale) / 2.0f,
      position.y - (tex.height * scale) / 2.0f
    };
    DrawTextureEx(tex, draw_pos, 0.0f, scale, WHITE);
  } else {
    Color c = (type == EnemyType::TYPE_1) ? GREEN : ORANGE;
    DrawRectangle((int)position.x - WIDTH / 2, (int)position.y - HEIGHT / 2,
                  WIDTH, HEIGHT, c);
  }
}

void Enemy::spawnBullet(std::array<Bullet, MAX_BULLETS> &bullets) const {
  for (auto &b : bullets) {
    if (!b.active) {
      b.position = {position.x, position.y + Enemy::HEIGHT / 2};
      b.velocity = {0.0f, Bullet::SPEED};
      b.active = true;
      b.type = BulletType::ENEMY;
      return;
    }
  }
}