#include "elements/text_input.h"
#include "shared/constants.h"

void TextInput::init(std::string *textRef, std::size_t maxLength,
                     std::size_t minLength) {
  text_ = textRef;
  max_length_ = maxLength;
  min_length_ = minLength;
}

bool isCharValid(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

void TextInput::update(int charPressed, std::function<int()> requestNextChar) {
  if (text_->empty() || text_->size() < min_length_) {
    err_ = "Input too short!";
  } else {
    err_ = "";
  }

  if (!is_focused_)
    return;

  while (charPressed > 0) {
    err_ = "";
    if (isCharValid((char)charPressed)) {
      if (text_->size() < max_length_) {
        text_->push_back((char)charPressed);
      }
    }

    charPressed = requestNextChar();
  }

  if (IsKeyPressed(KEY_BACKSPACE)) {
    if (!text_->empty())
      text_->pop_back();

    if (text_->size() < min_length_) {
      err_ = "Input too short!";
    } else {
      err_ = "";
    }
  }
}

float TextInput::draw() const {
  float offset = 0;

  DrawRectangleRec(rect, Fade(LIGHTGRAY, 0.5f));
  DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width,
                     (int)rect.height, is_focused_ ? RED : WHITE);

  DrawText(text_->c_str(), (int)rect.x + 5, (int)rect.y + 5, 40, WHITE);
  offset += rect.height;

  offset += 20.0f;
  const auto nameLengthText =
      TextFormat("INPUT CHARS: %i/%i", text_->size(), max_length_);
  DrawText(nameLengthText,
           (shared::SCREEN_WIDTH - MeasureText(nameLengthText, 20)) / 2.0f,
           rect.y + offset, 20, WHITE);
  offset += 30.0f;

  if (!err_.empty()) {
    DrawText(err_.c_str(),
             (shared::SCREEN_WIDTH - MeasureText(err_.c_str(), 20)) / 2.0f,
             rect.y + offset, 20, RED);

    offset += 20.0f;
  }

  return offset;
}

const std::string &TextInput::getError() const { return err_; }

void TextInput::setFocus(bool focus) { is_focused_ = focus; }
