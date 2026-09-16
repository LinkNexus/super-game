#include "utils.h"
#include "raylib.h"
#include "shared/constants.h"

shared::Vec2D toScreen(shared::Vec2D vec) {
  return {vec.x, shared::SCREEN_HEIGHT - vec.y};
}

Vector2 toRaylibVec(shared::Vec2D vec) {
  return {vec.x, shared::SCREEN_HEIGHT - vec.y};
}
