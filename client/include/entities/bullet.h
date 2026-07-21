#pragma once

#include "shared/messages.h"

enum class BulletType { PLAYER, ENEMY };

struct Bullet {
  void draw(const shared::BulletState &state) const;
};
