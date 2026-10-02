#pragma once

#include "raylib.h"
#include <cstddef>
#include <functional>
#include <string>

class TextInput {
private:
  std::string *text_{};
  std::size_t max_length_{};
  std::size_t min_length_{};
  std::string err_{};
  bool is_focused_{false};

public:
  Rectangle rect{};
  static constexpr float HEIGHT = 50.0f;
  static constexpr float WIDTH = 250.0f;

public:
  void init(std::string *textPtr, std::size_t maxLength = 256,
            std::size_t minLength = 1);
  float draw() const;
  void update(int charPressed, std::function<int()> requestNextChar);
  void setFocus(bool focus);
  const std::string &getError() const;
};
