#include "objects/star.h"
#include "constants.h"

void Star::init_random(int screen_width, int screen_height) {
  position.x = static_cast<float>(GetRandomValue(0, screen_width));
  position.y = static_cast<float>(GetRandomValue(0, screen_height));

  int depth  = GetRandomValue(1, 3);
  size       = depth * 0.7f;
  speed      = depth * 40.0f;

  unsigned char brightness = static_cast<unsigned char>(100 + depth * 50);
  color = {brightness, brightness, brightness, 255};
}

void Star::update(float dt, int screen_height, int screen_width) {
  position.y += speed * dt;
  if (position.y > screen_height) {
    position.x = static_cast<float>(GetRandomValue(0, screen_width));
    position.y = 0;
  }
}

void Star::draw() const {
  DrawCircleV(position, size, color);
}
