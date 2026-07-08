#include "Star.h"
#include <cstdlib>

void Star::init_random(int screenWidth, int screenHeight) {
  position.x = static_cast<float>(GetRandomValue(0, screenWidth));
  position.y = static_cast<float>(GetRandomValue(0, screenHeight));

  int depth = GetRandomValue(1, 3); // 1 = fern, 3 = nah
  size = depth * 0.7f;
  speed = depth * 40.0f;

  unsigned char brightness = static_cast<unsigned char>(100 + depth * 50);
  color = {brightness, brightness, brightness, 255};
}

void Star::update(float dt, int screenHeight, int screenWidth) {
  position.y += speed * dt;
  if (position.y > screenHeight) {
    position.x = static_cast<float>(GetRandomValue(0, screenWidth));
    position.y = 0;
  }
}

void Star::draw() {
  DrawCircleV(position, size, color);
}

void InitStarfield(Star stars[], int count, int screenWidth, int screenHeight) {
  for (int i = 0; i < count; i++) {
    stars[i].init_random(screenWidth, screenHeight);
  }
}

void UpdateStarfield(Star stars[], int count, float dt, int screenHeight, int screenWidth) {
  for (int i = 0; i < count; i++) {
    stars[i].update(dt, screenHeight, screenWidth);
  }
}

void DrawStarfield(Star stars[], int count) {
  for (int i = 0; i < count; i++) {
    stars[i].draw();
  }
}