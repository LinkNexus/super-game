#pragma once
#include "raylib.h"

struct Player {
  Vector2 position;
  float rotation = 0.0f;
  float speed = 300.0f;
  float size = 20.f;
  Texture2D texture;

  void load_texture();
  void update(float dt);
  void draw();
  void unload();
};