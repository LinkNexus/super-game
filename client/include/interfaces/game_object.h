#include "raylib.h"

struct GameObject {
public:
  GameObject(Vector2 position) : position_(position) {}
  virtual ~GameObject() = default;

  virtual void draw() const = 0;
  Vector2 getPosition() const { return position_; }
  int getId() const { return id_; }

protected:
  Vector2 position_;

private:
  int id_;
};
