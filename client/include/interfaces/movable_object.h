#include "game_object.h"
#include "raylib.h"
#include <functional>

struct MovableObject : public GameObject {
  using OnPositionChangedCallback = std::function<void(MovableObject &)>;

public:
  MovableObject(Vector2 position) : GameObject(position) {}

  void setOnPositionChangedCallback(OnPositionChangedCallback callback) {
    this->on_position_changed_callback_ = std::move(callback);
  }

  void move(Vector2 direction, float dt) {
    this->position_.x += direction.x * dt;
    this->position_.y += direction.y * dt;

    if (this->on_position_changed_callback_) {
      this->on_position_changed_callback_(*this);
    }
  }

private:
  OnPositionChangedCallback on_position_changed_callback_;
};
