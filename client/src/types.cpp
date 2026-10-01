#include "types.h"
#include "shared/constants.h"

void Particle::update(float dt) {
  if (lifetime <= 0.0f)
    return;

  position.x += velocity.x * dt;
  position.y += velocity.y * dt;

  // sime damping and slight gravity
  velocity.x *= 0.98f;
  velocity.y *= 0.98f;
  velocity.y += 20.0f * dt;

  lifetime -= dt;
  if (lifetime < 0.0f)
    lifetime = 0.0f;
}

void Star::initRandom() {
  position.x = static_cast<float>(GetRandomValue(0, shared::SCREEN_WIDTH));
  position.y = static_cast<float>(GetRandomValue(0, shared::SCREEN_HEIGHT));

  int depth = GetRandomValue(1, 3);
  size = depth * 0.7f;
  speed = depth * 40.0f;

  unsigned char brightness = static_cast<unsigned char>(100 + depth * 50);
  color = {brightness, brightness, brightness, 255};
}

void Star::update(float dt) {
  position.y += speed * dt;
  if (position.y > shared::SCREEN_HEIGHT) {
    position.x = static_cast<float>(GetRandomValue(0, shared::SCREEN_WIDTH));
    position.y = 0;
  }
}
