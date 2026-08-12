#pragma once

#include "constants.h"
#include "raylib.h"
#include "session.h"
#include "shared/constants.h"
#include "shared/messages.h"
#include <string>

struct Player {
  Texture2D texture = {};

  void loadTexture();
  void unload();

  static Texture2D heart_texture_;
  static constexpr float HEART_SIZE = 32.0f;
  static constexpr float HEART_SPACING = 4.0f;

private:
  void drawPlayer(const shared::Vec2D position, bool main) const;
  void drawLives(uint8_t lives, float y_offset) const;

public:
  template <std::size_t N>
  void
  draw(Session *current_session,
       const std::array<std::optional<shared::PlayerState>, N> &states) const {
    float y_offset = 10.0f;

    for (std::size_t idx = 0; idx < states.size(); ++idx) {
      const auto &player = states[idx];

      bool is_online_session = false;
      uint32_t player_id = 1;

      if (OnlineSession *session =
              dynamic_cast<OnlineSession *>(current_session)) {
        is_online_session = true;
        player_id = session->getPlayerId();
      }

      if (player.has_value()) {
        if (player->lives > 0)
          drawPlayer(player->position, player->id == player_id);

        const std::string playerLabel =
            (player->name.empty() ? "Player " + std::to_string(idx + 1)
                                  : player->name) +
            (!is_online_session ? ""
                                : ((player->id == player_id ? " (You)" : "")));
        DrawText(playerLabel.c_str(),
                 shared::SCREEN_WIDTH - 10 -
                     MeasureText(playerLabel.c_str(), STATUS_FONT_SIZE),
                 y_offset, STATUS_FONT_SIZE, WHITE);
        y_offset += STATUS_FONT_SIZE;

        drawLives(player->lives, y_offset);
        y_offset += HEART_SIZE;

        const char *points_text = TextFormat("Points: %d", player->points);
        DrawText(points_text,
                 shared::SCREEN_WIDTH - 10 -
                     MeasureText(points_text, STATUS_FONT_SIZE),
                 y_offset, STATUS_FONT_SIZE, WHITE);

        y_offset += STATUS_FONT_SIZE + 5;

        DrawLine(shared::SCREEN_WIDTH -
                     MeasureText(points_text, STATUS_FONT_SIZE) - 10,
                 y_offset, shared::SCREEN_WIDTH, y_offset, WHITE);

        y_offset += 25;
      }
    }
  }
};
