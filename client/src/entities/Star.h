#pragma once
#include "raylib.h"

struct Star {
  Vector2 position;
  float speed;
  float size;
  Color color;

  void init_random(int screenWidth, int screenHeight);
  void update(float dt, int screenHeight, int screenWidth);
  void draw();
};

void InitStarfield(Star stars[], int count, int screenWidth, int screenHeight);
void UpdateStarfield(Star stars[], int count, float dt, int screenHeight, int screenWidth);
void DrawStarfield(Star stars[], int count);